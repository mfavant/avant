// g++ -std=c++20 fifo.test.cpp -o fifo.exe -lpthread
#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

#include "../external/avant-ipc/fifo.h"

void sigpipe_handler(int signum)
{
    std::cerr << "SIGPIPE caught: " << signum << std::endl;
}

int main()
{
    signal(SIGPIPE, sigpipe_handler);

    constexpr int buffer_size = 512;

    std::thread aThread([]() -> void
                        {
                            avant::ipc::fifo fifo_instance_a("/tmp/a2b_path",
                                                             "/tmp/b2a_path",
                                                             avant::ipc::fifo::AUTH_A,
                                                             [](avant::ipc::fifo &_this) -> void
                                                             {
                                                                 std::cout << "destroy_callback a" << std::endl;
                                                             });
                            fifo_instance_a.init();

                            std::vector<char> send_buffer(buffer_size);
                            std::vector<char> recv_buffer(buffer_size + 1); // +1 for the '\0'

                            snprintf(send_buffer.data(), buffer_size, "Message from A to B");
                            while (true)
                            {
                                if (fifo_instance_a.write(send_buffer.data(), strlen(send_buffer.data())) < 0)
                                {
                                    std::cout << "A write err " << strerror(errno) << std::endl;
                                }
                                int len = fifo_instance_a.recv(recv_buffer.data(), buffer_size);
                                if (len > 0)
                                {
                                    recv_buffer[len] = '\0';
                                    std::cout << "A received: " << recv_buffer.data() << std::endl;
                                }
                                else if (len < 0)
                                {
                                    std::cout << "A recv err " << strerror(errno) << std::endl;
                                }
                                else
                                {
                                    std::cout << "A recv: nothing available" << std::endl;
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
                                                                 std::cout << "destroy_callback b" << std::endl;
                                                             });
                            fifo_instance_b.init();

                            std::vector<char> send_buffer(buffer_size);
                            std::vector<char> recv_buffer(buffer_size + 1); // +1 for the '\0'

                            snprintf(send_buffer.data(), buffer_size, "Message from B to A");
                            while (true)
                            {
                                if (fifo_instance_b.write(send_buffer.data(), strlen(send_buffer.data())) < 0)
                                {
                                    std::cout << "B write err " << strerror(errno) << std::endl;
                                }
                                int len = fifo_instance_b.recv(recv_buffer.data(), buffer_size);
                                if (len > 0)
                                {
                                    recv_buffer[len] = '\0';
                                    std::cout << "B received: " << recv_buffer.data() << std::endl;
                                }
                                else if (len < 0)
                                {
                                    std::cout << "B recv err " << strerror(errno) << std::endl;
                                }
                                else
                                {
                                    std::cout << "B recv: nothing available" << std::endl;
                                }
                                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            } });

    aThread.join();
    bThread.join();

    return 0;
}
