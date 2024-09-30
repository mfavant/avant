#pragma once
#include <string>
#include <functional>
#include <openssl/ssl.h>
#include <arpa/inet.h>
#include <endian.h>

namespace avant::socket
{
    class socket
    {
    public:
        virtual ~socket();
        socket();
        socket(const std::string &ip, int port);

        socket(const socket &) = delete;
        socket &operator=(const socket &) = delete;

        socket(socket &&other);

        socket &operator=(socket &&other);

        bool bind(const std::string &ip, int port);

        /**
         * @brief socket listen
         *
         * @param backlog The maximum length of the pending connection queue
         * @return true
         * @return false
         */
        bool listen(int backlog);

        bool connect(const std::string &ip, int port);
        bool close();
        int accept();
        int recv(char *buf, size_t len, int &oper_errno);
        int send(const char *buf, size_t len, int &oper_errno);
        bool set_non_blocking();
        bool set_blocking();
        bool set_send_buffer(size_t size);
        bool set_recv_buffer(size_t size);
        bool set_linger(bool active, size_t seconds);
        /**
         * @brief heart beat config
         *
         * @return true
         * @return false
         */
        bool set_keep_alive();
        bool set_reuse_addr();
        bool set_reuse_port();
        int get_fd();
        inline void set_fd(int fd)
        {
            this->m_sockfd = fd;
        }

        SSL *get_ssl_instance();
        void set_ssl_instance(SSL *ssl_instance);
        bool get_ssl_accepted();
        void set_ssl_accepted(bool accepted);

    protected:
        std::string m_ip{};
        int m_port{0};
        int m_sockfd{0};
        SSL *m_ssl_instance{nullptr};
        bool m_ssl_accepted{false};

    public:
        std::function<void()> close_callback{nullptr};

    public:
        static int create_tcp_socket(std::string ip);
        static bool is_ipv6(std::string ip);
    };
}