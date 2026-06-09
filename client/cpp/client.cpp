// linux
// g++ -o client.exe client.cpp ../../protocol/proto_res/*.pb.cc --std=c++17 -lprotobuf -lpthread
// macos
// CMakeLists.txt

#include <iostream>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <thread>
#include <chrono>
#include <unistd.h>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/message.h>

#if defined(__linux__)
#include <sys/epoll.h>
#include <endian.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#include <libkern/OSByteOrder.h>
#endif

#include <cstring>
#include <vector>
#include <fcntl.h>
#include <atomic>

#include "../../protocol/proto_res/proto_cmd.pb.h"
#include "../../protocol/proto_res/proto_example.pb.h"
#include "../../protocol/proto_res/proto_message_head.pb.h"

using namespace avant;

static bool is_ipv6(std::string ip)
{
    if (std::string::npos != ip.find_first_of('.'))
    {
        return false;
    }
    return true;
}

struct conn
{
    int fd{0};
    int sended{0};
    char recv_buffer[30]{0};
    uint64_t recv_data_len = 0;
    int status{0}; // 0 发 1读
};

int main(int argc, const char **argv)
{
    if (argc < 2)
    {
        std::cout << "arg<2" << std::endl;
        return 1;
    }

    // const char *server_ip = "61.171.51.135";
    const char *server_ip = "45.205.26.156";
    int server_port = ::atoi(argv[1]);

    int n;
    bool stop_flag = false;

    constexpr int thread_count = 2;

    constexpr int client_cnt = 10;

    constexpr int pingpong_cnt = 2;
    // 350KB/s

    std::atomic<uint64_t> recv_size = 0;

    // 1024KB
    for (int loop = 0; loop < thread_count; loop++)
    {
        // std::cout << "start thread " << loop << std::endl;
        std::thread m_thread(
            [server_port, server_ip, argv, loop, &stop_flag, &client_cnt, &pingpong_cnt, &recv_size]()
            {
                std::atomic<uint64_t> last_print_size = 0;
                ProtoPackage message;
                ProtoCSReqExample exampleReq;
                constexpr uint send_all_string_bytes = 8;
                std::string send_str(send_all_string_bytes, 'a');
                // send_str = argv[1];
                exampleReq.set_testcontext(send_str);
                message.set_cmd(ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE);
                // 30MB * 2000 = 60 000MB = 60GB
                uint64_t send_size = 0;

                uint64_t last_time = std::time(nullptr);

                int client_socket_arr[client_cnt]{0};
                conn conn_arr[client_cnt];

#ifdef __linux__
                struct epoll_event event, events[client_cnt];
                // epoll
                int epoll_fd = epoll_create(client_cnt);
                if (epoll_fd == -1)
                {
                    std::cerr << "epoll_create1 failed" << std::endl;
                    return;
                }
#elif defined(__APPLE__)
                struct kevent event, events[client_cnt];
                // kqueue
                int kq = kqueue();
                if (kq == -1)
                {
                    std::cerr << "failed to create kqueue" << std::endl;
                    return;
                }
#endif
                // std::cout << "start connect server " << server_ip << ":" << server_port << std::endl;
                for (int client_idx = 0; client_idx < client_cnt; client_idx++)
                {
                    int &client_socket = client_socket_arr[client_idx];

                    int INET = AF_INET;
                    if (is_ipv6(server_ip))
                    {
                        INET = AF_INET6;
                    }

                    client_socket = socket(INET, SOCK_STREAM, 0);
                    if (client_socket == -1)
                    {
                        perror("Socket creation failed");
                        return;
                    }

                    if (INET == AF_INET)
                    {
                        sockaddr_in server_address;
                        server_address.sin_family = AF_INET;
                        server_address.sin_port = htons(server_port);
                        server_address.sin_addr.s_addr = inet_addr(server_ip);

                        if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
                        {
                            perror("Connection failed");
                            close(client_socket);
                            return;
                        }
                    }
                    else
                    {
                        sockaddr_in6 server_address;
                        server_address.sin6_family = AF_INET6;
                        server_address.sin6_port = htons(server_port);
                        if (inet_pton(AF_INET6, server_ip, &server_address.sin6_addr) <= 0)
                        {
                            perror("Invalid IPV6 address");
                            close(client_socket);
                            return;
                        }

                        if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
                        {
                            perror("Connection failed");
                            close(client_socket);
                            return;
                        }
                    }

                    {
                        // 在创建套接字后立即设置为非阻塞模式
                        int flags = fcntl(client_socket, F_GETFL, 0);
                        if (flags == -1)
                        {
                            perror("fcntl F_GETFL");
                            exit(EXIT_FAILURE);
                        }
                        if (fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) == -1)
                        {
                            perror("fcntl F_SETFL O_NONBLOCK");
                            exit(EXIT_FAILURE);
                        }
                    }

                    conn_arr[client_idx].fd = client_socket;
                    conn_arr[client_idx].recv_data_len = 0;
                    conn_arr[client_idx].sended = 0;

#ifdef __linux__
                    // 添加fd到epoll
                    event.events = EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
                    // event.data.fd = client_socket;
                    event.data.ptr = &conn_arr[client_idx];
                    // printf("EPOLL ADD fd %d\n", client_socket);
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &event) == -1)
                    {
                        std::cerr << "epoll_ctl failed" << std::endl;
                        ::close(client_socket);
                        ::close(epoll_fd);
                        return;
                    }
#elif defined(__APPLE__)
                    // 添加fd到kqueue
                    EV_SET(&event, client_socket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, &conn_arr[client_idx]);
                    if (kevent(kq, &event, 1, NULL, 0, NULL) == -1)
                    {
                        std::cerr << "failed to add socket to kqueue" << std::endl;
                        ::close(client_socket);
                        ::close(kq);
                        return;
                    }
#endif
                }
                // std::cout << "connect server success " << client_cnt << " clients" << std::endl;

                std::string str;
                exampleReq.SerializeToString(&str);
                message.set_protocol(str);
                message.set_cmd(ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE);
                std::string data;
                message.SerializeToString(&data);

                uint64_t packageLen = data.size();
#ifdef __linux__
                packageLen = ::htobe64(packageLen);
#elif defined(__APPLE__)
                packageLen = OSSwapHostToBigInt64(packageLen);
#endif
                data.insert(0, (char *)&packageLen, sizeof(packageLen));

                while (true)
                {
#ifdef __linux__
                    int nfds = epoll_wait(epoll_fd, events, client_cnt, 10);
                    if (nfds == -1 && errno != EINTR)
                    {
                        perror("epoll_wait err");
                        exit(EXIT_FAILURE);
                    }
#elif defined(__APPLE__)
                    struct timespec timeout;
                    timeout.tv_sec = 1;
                    timeout.tv_nsec = 0; // 10 * 1000 * 1000; // 10ms
                    int nfds = kevent(kq, NULL, 0, events, client_cnt, &timeout);
                    if (nfds == -1 && errno != EINTR)
                    {
                        perror("kevent err nfds==-1");
                        exit(EXIT_FAILURE);
                    }
#endif

                    // std::cout << "loop " << loop << " nfds " << nfds << std::endl;

                    if (stop_flag)
                    {
                        for (int i = 0; i < client_cnt; i++)
                        {
                            if (client_socket_arr[i] > 0)
                            {
                                close(client_socket_arr[i]);
                            }
                        }
                        return;
                    }

                    for (int event_idx = 0; event_idx < nfds; event_idx++)
                    {
#ifdef __linux__
                        conn *client_conn = (conn *)events[event_idx].data.ptr;
#elif defined(__APPLE__)
                        conn *client_conn = (conn *)events[event_idx].udata;
#endif

                        int client_socket = client_conn->fd;

#ifdef __linux__
                        if (events[event_idx].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP))
                        {
                            perror("EPOLLERR | EPOLLRDHUP | EPOLLHUP");
                            exit(EXIT_FAILURE);
                        }
#elif defined(__APPLE__)
                        if (events[event_idx].flags & (EV_ERROR | EV_EOF))
                        {
                            perror("EV_ERROR");
                            exit(EXIT_FAILURE);
                        }
#endif

#ifdef __linux__
                        while ((events[event_idx].events & EPOLLOUT) && client_conn->status == 0)
#elif defined(__APPLE__)
                        while ((events[event_idx].filter == EVFILT_WRITE) && client_conn->status == 0)
#endif
                        {
                            // std::cout << "try send " << client_socket << std::endl;
                            while (data.size() != client_conn->sended)
                            {
                                int len = send(client_socket, data.c_str() + client_conn->sended, data.size() - client_conn->sended, 0);
                                if (len <= 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                                {
                                    perror("Send Head failed");
                                    printf(" errno %d", errno);
                                    exit(EXIT_FAILURE);
                                }
                                else if (len <= 0)
                                {
                                    break;
                                }
                                else if (len > 0)
                                {
                                    client_conn->sended += len;
                                }
                            }
                            // send over
                            if (data.size() == client_conn->sended)
                            {
                                client_conn->sended = 0;
                                client_conn->recv_data_len = 0;
                                client_conn->status = 1; // 监听读

#ifdef __linux__
                                event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
                                // event.data.fd = client_socket;
                                event.data.ptr = client_conn;
                                if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_socket, &event) == -1)
                                {
                                    std::cerr << "epoll_ctl failed" << std::endl;
                                    exit(EXIT_FAILURE);
                                }
#elif defined(__APPLE__)
                                // 修改kqueue事件为监听读事件
                                EV_SET(&event, client_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, client_conn);
                                if (kevent(kq, &event, 1, NULL, 0, NULL) == -1)
                                {
                                    std::cerr << "failed to add socket to kqueue" << std::endl;
                                    exit(EXIT_FAILURE);
                                }
                                // 暂时停止监听写事件
                                EV_SET(&event, client_socket, EVFILT_WRITE, EV_DISABLE, 0, 0, client_conn);
                                if (kevent(kq, &event, 1, NULL, 0, NULL) == -1)
                                {
                                    std::cerr << "failed to disable write event in kqueue" << std::endl;
                                    exit(EXIT_FAILURE);
                                }
#endif
                                // printf("send a package\n");
                                // std::cout << "send a package " << client_socket << std::endl;
                                break;
                            }
                            break;
                        }

#ifdef __linux__
                        while ((events[event_idx].events & EPOLLIN) && client_conn->status == 1)
#elif defined(__APPLE__)
                        while ((events[event_idx].filter == EVFILT_READ) && client_conn->status == 1)
#endif
                        {
                            // std::cout << "try read " << client_socket << std::endl;
                            while (22 != client_conn->recv_data_len)
                            {
                                int read_len = read(client_socket, client_conn->recv_buffer + client_conn->recv_data_len, sizeof(client_conn->recv_buffer));
                                // printf("read len %d", read_len);
                                if (read_len <= 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                                {
                                    perror("Read Head failed");
                                    exit(EXIT_FAILURE);
                                }
                                else if (read_len <= 0)
                                {
                                    break;
                                }
                                else
                                {
                                    client_conn->recv_data_len += read_len;
                                }
                            }
                            // read over
                            if (22 == client_conn->recv_data_len)
                            {
                                // 解包处理
                                {
                                    if (client_conn->recv_data_len < sizeof(uint64_t))
                                    {
                                        perror("client_conn->recv_data_len < sizeof(uint64_t)");
                                        exit(EXIT_FAILURE);
                                    }
#ifdef __linux__
                                    uint64_t packSize = ::be64toh(*((uint64_t *)client_conn->recv_buffer));
#elif defined(__APPLE__)
                                    uint64_t rawPackSize;
                                    memcpy(&rawPackSize, client_conn->recv_buffer, sizeof(rawPackSize));
                                    uint64_t packSize = OSSwapBigToHostInt64(rawPackSize);
#endif
                                    if (client_conn->recv_data_len < packSize + sizeof(uint64_t))
                                    {
                                        perror("client_conn->recv_data_len < packSize + sizeof(uint64_t)");
                                        exit(EXIT_FAILURE);
                                    }

                                    ProtoPackage protoPackage;
                                    if (!protoPackage.ParseFromArray(client_conn->recv_buffer + sizeof(uint64_t), packSize))
                                    {
                                        perror("protoPackage.ParseFromArray err");
                                        exit(EXIT_FAILURE);
                                    }
                                    else
                                    {
                                        if (protoPackage.cmd() == ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE)
                                        {
                                            ProtoCSResExample exampleRes;
                                            if (exampleRes.ParseFromString(protoPackage.protocol()))
                                            {
                                                recv_size.fetch_add(1);

                                                std::cout << "recv a package " << client_socket << " recv_size " << recv_size.load() << std::endl;

                                                // uint64_t now_time = std::time(nullptr);
                                                if ((recv_size - last_print_size) >= 100000)
                                                {
                                                    // std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                                                    // std::chrono::milliseconds recv_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
                                                    std::cout << "pingpong cnt " << recv_size << "  " << send_all_string_bytes << " bytes " << std::endl;
                                                    last_print_size.store(recv_size);
                                                }
                                                // last_time = now_time;
                                                // printf("recv a package\n");
                                            }
                                            else
                                            {
                                                perror("exampleRes.ParseFromString err");
                                                exit(EXIT_FAILURE);
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            perror("protoPackage.cmd() != ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE");
                                            exit(EXIT_FAILURE);
                                        }
                                    }
                                }

                                client_conn->sended = 0;
                                client_conn->recv_data_len = 0;
                                client_conn->status = 0; // 监听发

#ifdef __linux__
                                event.events = EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
                                // event.data.fd = client_socket;
                                event.data.ptr = client_conn;
                                if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_socket, &event) == -1)
                                {
                                    std::cerr << "epoll_ctl failed" << std::endl;
                                    exit(EXIT_FAILURE);
                                }
#elif defined(__APPLE__)
                                // 修改kqueue事件为监写事件
                                EV_SET(&event, client_socket, EVFILT_WRITE, EV_ENABLE, 0, 0, client_conn);
                                if (kevent(kq, &event, 1, NULL, 0, NULL) == -1)
                                {
                                    std::cerr << "failed to disable write event in kqueue" << std::endl;
                                    exit(EXIT_FAILURE);
                                }
                                // 暂时停止监听读事件
                                EV_SET(&event, client_socket, EVFILT_READ, EV_DISABLE, 0, 0, client_conn);
                                if (kevent(kq, &event, 1, NULL, 0, NULL) == -1)
                                {
                                    std::cerr << "failed to disable read event in kqueue" << std::endl;
                                    exit(EXIT_FAILURE);
                                }
#endif

                                // printf("recv over,loop status to send");
                                break;
                            }
                            break;
                        }
                    } // for event num

                } // while(true);
            });
        m_thread.detach();
    }

    std::cin >> n;
    stop_flag = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    return 0;
}
