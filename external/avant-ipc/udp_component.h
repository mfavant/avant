#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <string>

#include <functional>

namespace avant
{
    namespace ipc
    {
        int udp_component_setnonblocking(int fd);

        class udp_component
        {
        public:
            ~udp_component();

            // 使用 sockaddr_storage 来确保 IPv6/IPv4 地址都能安全存储
            std::string udp_component_get_ip(const struct sockaddr_storage &addr);
            int udp_component_get_port(const struct sockaddr_storage &addr);

            // 如果 IP 为空字符串 "" 表示绑定到 any (:: or 0.0.0.0 取决于 socket 类型)
            // start_event_loop=false 时只创建 socket 并 bind，返回 0/-1，
            // 调用方自行通过 get_socket_fd() 将 fd 加入自己的事件循环
            int udp_component_server(const std::string &IP,
                                     const int PORT,
                                     bool start_event_loop = true);

            // client：如果 addr != nullptr 则向指定地址发送并返回（用于服务器向客户端反包），
            // 否则向 TARGET_IP:TARGET_PORT 发送（用于客户端向服务器发送消息）。
            // 如果 socket 尚未创建则按 addr 或 TARGET_IP 的地址族自动创建。
            int udp_component_client(
                const std::string &TARGET_IP,
                const int TARGET_PORT,
                const char *buffer,
                ssize_t len,
                struct sockaddr *addr = nullptr,
                socklen_t addr_len = 0);

            static bool is_ipv6(const std::string &ip);

            // 阻塞事件循环（epoll/kqueue），由 tick_callback 控制退出
            int event_loop();
            // 一次性读尽当前可读的 UDP 报文并逐个调用 message_callback
            int server_recvfrom(unsigned int max_loop);

            int get_socket_fd();

        private:
            int create_socket(int family);
            int init_sock(const std::string &ip);
            void to_close();

        private:
            // 用 -1 表示无效 fd
            int m_socket_fd{-1};
            int m_epoll_or_kqueue_fd{-1};
            // event_loop 是否已把 socket 注册进自己的 epoll/kqueue 实例，
            // 防止重复注册导致误关 socket
            bool m_event_registered{false};
            // server_recvfrom 的复用缓冲区，避免每次调用都堆分配
            std::unique_ptr<char[]> m_recv_buffer{nullptr};

        public:
            // tick_callback: 可在 event loop 中周期性调用来判断是否退出
            std::function<void(bool &to_stop)> tick_callback{nullptr};

            // message callback: 安全传入 sockaddr_storage（拷贝/引用都安全）
            std::function<void(const char *buffer, ssize_t len, const struct sockaddr_storage &addr, socklen_t addr_len)> message_callback{nullptr};

            std::function<void()> close_callback{nullptr};
        };
    } // namespace ipc
} // namespace avant
