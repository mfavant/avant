#include "udp_component.h"
#include <memory>

namespace avant
{
    namespace ipc
    {
        int udp_component_setnonblocking(int fd)
        {
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags == -1)
            {
                perror("udp_component_setnonblocking: error getting fd flags");
                return -1;
            }
            if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            {
                perror("udp_component_setnonblocking: error setting non-blocking mode");
                return -1;
            }
            return 0;
        }

        udp_component::~udp_component()
        {
            to_close();
        }

        void udp_component::to_close()
        {
            if (m_socket_fd != -1)
            {
                close(m_socket_fd);
                m_socket_fd = -1;
            }
            if (m_epoll_fd != -1)
            {
                close(m_epoll_fd);
                m_epoll_fd = -1;
            }
            if (close_callback)
            {
                close_callback();
            }
        }

        int udp_component::init_sock(const std::string &ip)
        {
            if (m_socket_fd != -1)
            {
                return 0; // 已初始化
            }

            bool want_ipv6 = false;
            if (ip.empty())
            {
                // 默认使用 IPv6 socket（dual-stack），以便支持 IPv4 + IPv6
                want_ipv6 = true;
            }
            else
            {
                want_ipv6 = is_ipv6(ip);
            }

            if (want_ipv6)
            {
                m_socket_fd = ::socket(AF_INET6, SOCK_DGRAM, 0);
                if (m_socket_fd == -1)
                {
                    perror("init_sock: error creating AF_INET6 socket");
                    return -1;
                }
                // 允许接收 IPv4-mapped 地址（dual-stack）
                int off = 0;
                if (setsockopt(m_socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) == -1)
                {
                    // 非致命：记录但不强制失败（有的平台可能不允许修改）
                    perror("init_sock: warning setsockopt(IPV6_V6ONLY) failed");
                }
            }
            else
            {
                m_socket_fd = ::socket(AF_INET, SOCK_DGRAM, 0);
                if (m_socket_fd == -1)
                {
                    perror("init_sock: error creating AF_INET socket");
                    return -1;
                }
            }

            return 0;
        }

        std::string udp_component::udp_component_get_ip(const struct sockaddr_storage &addr)
        {
            char ipbuf[INET6_ADDRSTRLEN + 1] = {0};
            if (addr.ss_family == AF_INET)
            {
                struct sockaddr_in *a = (struct sockaddr_in *)&addr;
                inet_ntop(AF_INET, &a->sin_addr, ipbuf, INET_ADDRSTRLEN);
                return std::string(ipbuf);
            }
            else if (addr.ss_family == AF_INET6)
            {
                struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)&addr;
                inet_ntop(AF_INET6, &a6->sin6_addr, ipbuf, INET6_ADDRSTRLEN);
                return std::string(ipbuf);
            }
            return std::string();
        }

        int udp_component::udp_component_get_port(const struct sockaddr_storage &addr)
        {
            if (addr.ss_family == AF_INET)
            {
                struct sockaddr_in *a = (struct sockaddr_in *)&addr;
                return ntohs(a->sin_port);
            }
            else if (addr.ss_family == AF_INET6)
            {
                struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)&addr;
                return ntohs(a6->sin6_port);
            }
            return -1;
        }

        int udp_component::udp_component_server(const std::string &IP,
                                                const int PORT,
                                                bool start_event_loop /*= true*/)
        {
            const int int_port = PORT;

            // create udp socket (可能是 AF_INET6 dual-stack)
            if (m_socket_fd == -1)
            {
                if (0 != init_sock(IP))
                {
                    perror("udp_component_server: Error creating socket");
                    return -1;
                }
            }

            // 设置 socket 选项：在 bind 之前设置
            int reuse = 1;
            if (setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
            {
                perror("udp_component_server: warning setsockopt(SO_REUSEADDR) failed");
            }
#ifdef SO_REUSEPORT
            if (setsockopt(m_socket_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1)
            {
                // 非致命
                perror("udp_component_server: warning setsockopt(SO_REUSEPORT) failed");
            }
#endif

            // bind
            if (is_ipv6(IP) || IP.empty())
            {
                struct sockaddr_in6 server_addr6;
                memset(&server_addr6, 0, sizeof(server_addr6));
                server_addr6.sin6_family = AF_INET6;
                server_addr6.sin6_port = htons(int_port);
                if (!IP.empty())
                {
                    if (inet_pton(AF_INET6, IP.c_str(), &server_addr6.sin6_addr) != 1)
                    {
                        perror("udp_component_server: Invalid IPV6 address");
                        to_close();
                        return -1;
                    }
                }
                else
                {
                    server_addr6.sin6_addr = in6addr_any;
                }

                if (-1 == bind(m_socket_fd, (struct sockaddr *)&server_addr6, sizeof(server_addr6)))
                {
                    perror("udp_component_server: error binding AF_INET6 socket");
                    to_close();
                    return -1;
                }
            }
            else
            {
                struct sockaddr_in server_addr;
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(int_port);
                if (!IP.empty())
                {
                    if (inet_pton(AF_INET, IP.c_str(), &server_addr.sin_addr) != 1)
                    {
                        perror("udp_component_server: Invalid IPV4 address");
                        to_close();
                        return -1;
                    }
                }
                else
                {
                    server_addr.sin_addr.s_addr = INADDR_ANY;
                }

                if (-1 == bind(m_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
                {
                    perror("udp_component_server: error binding AF_INET socket");
                    to_close();
                    return -1;
                }
            }

            if (start_event_loop)
            {
                return event_loop();
            }
            return 0;
        }

        int udp_component::udp_component_client(
            const std::string &TARGET_IP,
            const int TARGET_PORT,
            const char *buffer,
            ssize_t len,
            struct sockaddr *addr /*=nullptr*/,
            socklen_t addr_len /*=0*/)
        {
            // ensure socket exists
            if (m_socket_fd == -1 && addr == nullptr)
            {
                if (0 != init_sock("")) // client udp socket
                {
                    perror("udp_component_client: Error creating client udp socket");
                    return -1;
                }
            }

            sockaddr_in parse_target_addr;
            memset(&parse_target_addr, 0, sizeof(parse_target_addr));
            sockaddr_in6 parse_target_addr6;
            memset(&parse_target_addr6, 0, sizeof(parse_target_addr6));

            // call by client
            if (TARGET_IP != "" && TARGET_PORT != 0)
            {
                if (is_ipv6(TARGET_IP))
                {
                    parse_target_addr6.sin6_family = AF_INET6;
                    if (inet_pton(AF_INET6, TARGET_IP.c_str(), &parse_target_addr6.sin6_addr) <= 0)
                    {
                        perror("udp_component_client: Invalid IPV6 address");
                        to_close();
                        return -1;
                    }
                    parse_target_addr6.sin6_port = htons(TARGET_PORT);
                }
                else
                {
                    parse_target_addr.sin_family = AF_INET;
                    if (inet_pton(AF_INET, TARGET_IP.c_str(), &parse_target_addr.sin_addr) != 1)
                    {
                        perror("udp_component_client: Invalid IPV4 address");
                        to_close();
                        return -1;
                    }
                    parse_target_addr.sin_port = htons(TARGET_PORT);
                }
            }
            else if (addr && addr_len > 0)
            {
                // 直接使用传入的 addr
            }
            else
            {
                perror("udp_component_client: IP PORT and addr addr_len cannot both be empty/zero");
                return -1;
            }

            ssize_t bytesSent = 0;

            if (addr) // 服务端调用向客户端反包
            {
                bytesSent = sendto(m_socket_fd, buffer, len, 0, addr, addr_len);
            }
            else
            {
                sockaddr *target_addr = (sockaddr *)&parse_target_addr;
                socklen_t target_len = sizeof(sockaddr_in);
                if (is_ipv6(TARGET_IP))
                {
                    target_addr = (sockaddr *)&parse_target_addr6;
                    target_len = sizeof(sockaddr_in6);
                }
                bytesSent = sendto(m_socket_fd, buffer, len, 0, target_addr, target_len);
            }

            if (bytesSent != len)
            {
                perror("udp_component_client: Error sending message (bytesSent != len)");
                // 不立即 close，返回错误以便上层处理
                return -1;
            }

            return 0;
        }

        int udp_component::event_loop()
        {
            if (m_socket_fd == -1)
            {
                perror("event_loop: err m_socket_fd == -1");
                return -1;
            }

            // set nonblocking
            if (udp_component_setnonblocking(m_socket_fd) != 0)
            {
                perror("event_loop: udp_component_setnonblocking err");
                to_close();
                return -1;
            }

            // create epoll instance if not created
            if (m_epoll_fd == -1)
            {
                m_epoll_fd = epoll_create1(0);
            }
            if (m_epoll_fd == -1)
            {
                perror("event_loop: error creating epoll");
                to_close();
                return -1;
            }

            // add server fd to epoll's listen list
            epoll_event event;
            memset(&event, 0, sizeof(event));
            event.data.fd = m_socket_fd;
            event.events = EPOLLIN | EPOLLET; // edge-triggered 更高效（注意：非阻塞）
            if (-1 == epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, m_socket_fd, &event))
            {
                perror("event_loop: error adding server socket to epoll");
                to_close();
                return -1;
            }

            constexpr int MAX_EVENTS_NUM = 8;
            epoll_event events[MAX_EVENTS_NUM];
            memset(events, 0, sizeof(events));

            // event loop
            while (true)
            {
                int timeout_ms = 1000; // 1s；避免 10ms 的 busy-loop
                int events_num = epoll_wait(m_epoll_fd, events, MAX_EVENTS_NUM, timeout_ms);
                if (events_num < 0)
                {
                    if (errno == EINTR)
                    {
                        continue;
                    }
                    perror("event_loop: epoll_wait error");
                    to_close();
                    return -1;
                }

                // tick
                if (tick_callback)
                {
                    bool to_stop = false;
                    tick_callback(to_stop);
                    if (to_stop)
                    {
                        to_close();
                        return 0;
                    }
                }

                if (events_num == 0)
                {
                    // timeout，无事件，继续循环（tick 已经执行）
                    continue;
                }

                for (int i = 0; i < events_num; ++i)
                {
                    if (events[i].data.fd == m_socket_fd)
                    {
                        server_recvfrom(10000);
                        // 可能有多个消息（edge-triggered），循环读尽
                    }
                } // end for events
            } // end while
            return 0;
        }

        int udp_component::server_recvfrom(unsigned int max_loop)
        {
            if (m_socket_fd < 0)
            {
                return -1;
            }
            if (max_loop == 0)
            {
                max_loop = UINT32_MAX;
            }

            constexpr int buffer_size = 65507;
            std::unique_ptr<char[]> buffer(new char[buffer_size]);

            unsigned int loop_count = 0;
            while (true)
            {
                if (loop_count >= max_loop)
                {
                    break;
                }
                loop_count++;

                struct sockaddr_storage client_addr;
                socklen_t addr_len = sizeof(client_addr);
                memset(&client_addr, 0, sizeof(client_addr));

                ssize_t bytes = recvfrom(m_socket_fd, buffer.get(), buffer_size, 0,
                                         (struct sockaddr *)&client_addr, &addr_len);
                if (bytes < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        // 已读尽
                        break;
                    }
                    else if (errno == EINTR)
                    {
                        continue;
                    }
                    else
                    {
                        perror("event_loop: recvfrom error");
                        // 不立即关闭，跳出当前 socket 读循环
                        break;
                    }
                }
                else if (bytes == 0)
                {
                    // UDP 不会正常返回 0，忽略
                    break;
                }
                else
                {
                    // 调用回调（传入 const sockaddr_storage &）
                    if (message_callback)
                    {
                        message_callback(buffer.get(), bytes, client_addr, addr_len);
                    }
                }
            } // end inner read loop

            return 0;
        }

        bool udp_component::is_ipv6(const std::string &ip)
        {
            if (ip.empty())
            {
                return true; // 默认使用 IPv6 socket（dual-stack）
            }
            struct in6_addr addr6;
            if (inet_pton(AF_INET6, ip.c_str(), &addr6) == 1)
            {
                return true;
            }
            struct in_addr addr4;
            if (inet_pton(AF_INET, ip.c_str(), &addr4) == 1)
            {
                return false;
            }
            // 如果既不是合法 IPv4 也不是合法 IPv6，则保守判断为 IPv6（上层会在 bind/pton 报错）
            return true;
        }

        int udp_component::get_socket_fd()
        {
            return this->m_socket_fd;
        }

    } // namespace ipc
} // namespace avant
