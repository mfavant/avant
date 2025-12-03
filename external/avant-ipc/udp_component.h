#pragma once

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <sys/epoll.h>
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
            int udp_component_server(const std::string &IP,
                                     const int PORT,
                                     bool start_event_loop = true);

            // client：如果 addr != nullptr 则向指定地址发送并返回，否则向 IP:PORT 发送并在发送完后进入 event_loop()，前者用于服务器向客户端反包 后者用于客户端向服务器发送消息
            int udp_component_client(
                const std::string &TARGET_IP,
                const int TARGET_PORT,
                const char *buffer,
                ssize_t len,
                struct sockaddr *addr = nullptr,
                socklen_t addr_len = 0);

            static bool is_ipv6(const std::string &ip);

            int event_loop();
            int server_recvfrom(unsigned int max_loop);

            int get_socket_fd();

        private:
            int init_sock(const std::string &ip);
            void to_close();

        private:
            // 用 -1 表示无效 fd
            int m_socket_fd{-1};
            int m_epoll_fd{-1};

        public:
            // tick_callback: 可在 event loop 中周期性调用来判断是否退出
            std::function<void(bool &to_stop)> tick_callback{nullptr};

            // message callback: 安全传入 sockaddr_storage（拷贝/引用都安全）
            std::function<void(const char *buffer, ssize_t len, const struct sockaddr_storage &addr, socklen_t addr_len)> message_callback{nullptr};

            std::function<void()> close_callback{nullptr};
        };
    }
}
