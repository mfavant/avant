// g++ main.cpp -o main.exe -lpthread -g
#include <iostream>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <functional>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <thread>
#include <chrono>
#include <csignal>

namespace avant
{
    namespace ipc
    {
        class fifo
        {
        public:
            enum iam
            {
                AUTH_A = 0,
                AUTH_B = 1,
                AUTH_NONE = 2
            };

            fifo(const std::string &a2b_path,
                 const std::string &b2a_path,
                 iam me,
                 std::function<void(fifo &)> destory_callback) : a2b_path(a2b_path),
                                                                 b2a_path(b2a_path),
                                                                 me(me),
                                                                 destory_callback(destory_callback)
            {
            }

            ~fifo()
            {
                if (destory_callback)
                {
                    destory_callback(*this);
                }
                if (fd_a2b > 0)
                {
                    close(fd_a2b);
                    unlink(a2b_path.c_str());
                }
                if (fd_b2a > 0)
                {
                    close(fd_b2a);
                    unlink(b2a_path.c_str());
                }
            }

            void init()
            {
                if (a2b_path.empty())
                {
                    throw std::runtime_error("a2b_path empty");
                }
                if (b2a_path.empty())
                {
                    throw std::runtime_error("b2a_path empty");
                }
                int ret = mkfifo(a2b_path.c_str(), 0777);
                if (ret == -1)
                {
                    if (errno != EEXIST)
                    {
                        throw std::runtime_error("mkfifo a2b_path ret -1 and errno != EEXIST");
                    }
                }
                ret = mkfifo(b2a_path.c_str(), 0777);
                if (ret == -1)
                {
                    if (errno != EEXIST)
                    {
                        throw std::runtime_error("mkfifo b2a_path ret -1 and errno != EEXIST");
                    }
                }

                if (me == AUTH_A)
                {
                    fd_b2a = open(b2a_path.c_str(), O_RDONLY);
                    if (fd_b2a <= 0)
                    {
                        throw std::runtime_error("AUTH_A open b2a err");
                    }

                    fd_a2b = open(a2b_path.c_str(), O_WRONLY);
                    if (fd_a2b <= 0)
                    {
                        throw std::runtime_error("AUTH_A open a2b err");
                    }
                }
                else if (me == AUTH_B)
                {
                    fd_b2a = open(b2a_path.c_str(), O_WRONLY);
                    if (fd_b2a <= 0)
                    {
                        throw std::runtime_error("AUTH_B open b2a err");
                    }

                    fd_a2b = open(a2b_path.c_str(), O_RDONLY);
                    if (fd_a2b <= 0)
                    {
                        throw std::runtime_error("AUTH_B open a2b err");
                    }
                }
                else
                {
                    throw std::runtime_error("unknow me, it is not AUTH_A or AUTH_B");
                }

                // set nonblock
                {
                    int flags = fcntl(fd_a2b, F_GETFL, 0);
                    if (0 != fcntl(fd_a2b, F_SETFL, flags | O_NONBLOCK))
                    {
                        throw std::runtime_error("set nonblock fd_a2b err");
                    }

                    flags = fcntl(fd_b2a, F_GETFL, 0);
                    if (0 != fcntl(fd_b2a, F_SETFL, flags | O_NONBLOCK))
                    {
                        throw std::runtime_error("set nonblock fd_b2a err");
                    }
                }

                init_succ = true;
            }

            int get_write_fd()
            {
                if (this->me == AUTH_A)
                {
                    return fd_a2b;
                }
                else if (this->me == AUTH_B)
                {
                    return fd_b2a;
                }
                else
                {
                    throw std::runtime_error("unknow me, it is not AUTH_A or AUTH_B");
                }
            }

            int get_read_fd()
            {
                if (this->me == AUTH_A)
                {
                    return fd_b2a;
                }
                else if (this->me == AUTH_B)
                {
                    return fd_a2b;
                }
                else
                {
                    throw std::runtime_error("unknow me, it is not AUTH_A or AUTH_B");
                }
            }

            bool is_init_succ()
            {
                return init_succ;
            }

            int write(const char *buffer, int len)
            {
                int total_written = 0;
                while (total_written < len)
                {
                    int written = ::write(get_write_fd(), buffer + total_written, len - total_written);
                    if (written == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            // 若非阻塞写入失败，等待片刻
                            std::cout << "write EAGAIN or EWOULDBLOCK" << std::endl;
                            continue;
                        }
                        else
                        {
                            return -1;
                        }
                    }
                    total_written += written;
                }
                return total_written;
            }

            int recv(char *buffer, int len)
            {
                int total_read = 0;
                while (total_read < len)
                {
                    int read_bytes = ::read(get_read_fd(), buffer + total_read, len - total_read);
                    if (read_bytes == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break; // 若非阻塞读取失败，等待片刻
                        }
                        else
                        {
                            return -1;
                        }
                    }
                    else if (read_bytes == 0)
                    {
                        break; // 管道关闭
                    }
                    total_read += read_bytes;
                }
                return total_read;
            }

        public:
            std::string get_a2b_path()
            {
                return a2b_path;
            }
            std::string get_b2a_path()
            {
                return b2a_path;
            }

        private:
            std::string a2b_path;
            std::string b2a_path;
            int fd_a2b{-1};
            int fd_b2a{-1};
            iam me{AUTH_NONE};
            bool init_succ{false};
            std::function<void(fifo &)> destory_callback{nullptr};
        };
    }
}

void sigpipe_handler(int signum)
{
    std::cerr << "SIGPIPE caught: " << signum << std::endl;
}

int main()
{
    signal(SIGPIPE, sigpipe_handler);

    std::thread aThread([]() -> void
                        {
                            avant::ipc::fifo fifo_instance_a("/tmp/a2b_path",
                                                             "/tmp/b2a_path",
                                                             avant::ipc::fifo::AUTH_A,
                                                             [](avant::ipc::fifo &_this) -> void
                                                             {
                                                                 std::cout << "destory_callback a" << std::endl;
                                                             });
                            fifo_instance_a.init();

                            constexpr int buffer_size = 512;
                            char send_buffer[buffer_size]{0};
                            char recv_buffer[buffer_size]{0};

                            snprintf(send_buffer, buffer_size, "Message from A to B");
                            while (true)
                            {
                                fifo_instance_a.write(send_buffer, strlen(send_buffer));
                                int len = fifo_instance_a.recv(recv_buffer, buffer_size);
                                if (len > 0)    {
                                    recv_buffer[len] = '\0';
                                    std::cout << "A received: " << recv_buffer << std::endl;
                                }
                                else
                                {
                                    std::cout << "A recv err " << len << std::endl;
                                }
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                            } });

    std::thread bThread([]() -> void
                        {
                            avant::ipc::fifo fifo_instance_b("/tmp/a2b_path",
                                                             "/tmp/b2a_path",
                                                             avant::ipc::fifo::AUTH_B,
                                                             [](avant::ipc::fifo &_this) -> void
                                                             {
                                                                 std::cout << "destory_callback b" << std::endl;
                                                             });
                            fifo_instance_b.init();

                            constexpr int buffer_size = 512;
                            char send_buffer[buffer_size]{0};
                            char recv_buffer[buffer_size]{0};

                            snprintf(send_buffer, buffer_size, "Message from B to A");
                            while (true)
                            {
                                fifo_instance_b.write(send_buffer, strlen(send_buffer));
                                int len = fifo_instance_b.recv(recv_buffer, buffer_size);
                                if (len > 0)
                                {
                                    recv_buffer[len] = '\0';
                                    std::cout << "B received: " << recv_buffer << std::endl;
                                }
                                else
                                {
                                    std::cout << "B recv err " << len << std::endl;
                                }
                                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            } });

    aThread.join();
    bThread.join();

    return 0;
}
