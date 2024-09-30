#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <avant-log/logger.h>

#include "socket/socket.h"
#include "utility/singleton.h"

using avant::socket::socket;
using namespace avant::utility;
using std::string;

socket::socket() : m_port(0), m_sockfd(0)
{
}

socket::socket(const string &ip, int port) : m_ip(ip), m_port(port), m_sockfd(0)
{
}

socket::socket(avant::socket::socket &&other)
{
    this->m_ip = other.m_ip;
    other.m_ip.clear();
    this->m_port = other.m_port;
    other.m_port = 0;
    this->m_sockfd = other.m_sockfd;
    other.m_sockfd = 0;
    this->m_ssl_accepted = other.m_ssl_accepted;
    other.m_ssl_accepted = false;
    this->m_ssl_instance = other.m_ssl_instance;
    other.m_ssl_instance = nullptr;

    this->close_callback = other.close_callback;
    other.close_callback = nullptr;
}

avant::socket::socket &socket::operator=(socket &&other)
{
    if (this == &other)
    {
        return *this;
    }

    this->m_ip = other.m_ip;
    other.m_ip.clear();
    this->m_port = other.m_port;
    other.m_port = 0;
    this->m_sockfd = other.m_sockfd;
    other.m_sockfd = 0;
    this->m_ssl_accepted = other.m_ssl_accepted;
    other.m_ssl_accepted = false;
    this->m_ssl_instance = other.m_ssl_instance;
    other.m_ssl_instance = nullptr;

    this->close_callback = other.close_callback;
    other.close_callback = nullptr;

    return *this;
}

socket::~socket()
{
    close();
}

bool socket::bind(const string &ip, int port)
{
    bool isIPV6 = is_ipv6(ip);

    struct sockaddr_in sockaddr; // IPV4
    memset(&sockaddr, 0, sizeof(sockaddr));
    struct sockaddr_in6 sockaddr6; // IPV6
    memset(&sockaddr6, 0, sizeof(sockaddr6));

    if (isIPV6)
    {
        // family
        sockaddr6.sin6_family = AF_INET6;
        // addr
        if (inet_pton(AF_INET6, ip.c_str(), &sockaddr6.sin6_addr) <= 0)
        {
            LOG_ERROR("Invalid IPV6 address");
            return false;
        }
        // port
        sockaddr6.sin6_port = htons(port);
        if (::bind(m_sockfd, (struct sockaddr *)&sockaddr6, sizeof(sockaddr6)) < 0)
        {
            LOG_ERROR("ipv6 socket bind error: errno=%d errstr=%s", errno, strerror(errno));
            return false;
        }
    }
    else
    {
        // family
        sockaddr.sin_family = AF_INET;
        // addr
        sockaddr.sin_addr.s_addr = inet_addr(ip.c_str());
        // port
        sockaddr.sin_port = htons(port);
        // bind
        if (::bind(m_sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
        {
            LOG_ERROR("ipv4 socket bind error: errno=%d errstr=%s", errno, strerror(errno));
            return false;
        }
    }

    return true;
}

bool socket::listen(int backlog)
{
    // backlog: queue of pending connections
    if (::listen(m_sockfd, backlog) < 0)
    {
        LOG_ERROR("socket listen error: errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::connect(const string &ip, int port)
{
    bool isIPV6 = is_ipv6(ip);
    int int_isIPV6 = isIPV6 ? 1 : 0;

    struct sockaddr_in sockaddr; // IPV4
    memset(&sockaddr, 0, sizeof(sockaddr));
    struct sockaddr_in6 sockaddr6; // IPV6
    memset(&sockaddr6, 0, sizeof(sockaddr6));

    if (isIPV6)
    {
        // family
        sockaddr6.sin6_family = AF_INET6;
        // addr
        if (inet_pton(AF_INET6, ip.c_str(), &sockaddr6.sin6_addr) <= 0)
        {
            LOG_ERROR("Invalid IPV6 address");
            return false;
        }
        // port
        sockaddr6.sin6_port = htons(port);
        if (::connect(m_sockfd, (struct sockaddr *)&sockaddr6, sizeof(sockaddr6)) < 0)
        {
            if (errno != EINPROGRESS)
            {
                LOG_ERROR("isIPV6 %d socket connect error: errno=%d errstr=%s", int_isIPV6, errno, strerror(errno));
                return false;
            }
        }
    }
    else
    {
        // family
        sockaddr.sin_family = AF_INET;
        // addr
        sockaddr.sin_addr.s_addr = inet_addr(ip.c_str());
        // port
        sockaddr.sin_port = htons(port);
        if (::connect(m_sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
        {
            if (errno != EINPROGRESS)
            {
                LOG_ERROR("isIPV6 %d socket connect error: errno=%d errstr=%s", int_isIPV6, errno, strerror(errno));
                return false;
            }
        }
    }

    return true;
}

bool socket::close()
{
    if (close_callback)
    {
        try
        {
            close_callback();
        }
        catch (...)
        {
            LOG_ERROR("close_callback throw eexception");
        }
    }

    if (m_ssl_instance)
    {
        SSL_shutdown(m_ssl_instance);
        SSL_free(m_ssl_instance);
        m_ssl_instance = nullptr;
    }
    m_ssl_accepted = false;

    if (m_sockfd > 0)
    {
        ::close(m_sockfd);
        m_sockfd = 0;
    }
    close_callback = nullptr;
    m_ip.clear();
    m_port = 0;

    return true;
}

int socket::accept()
{
    return ::accept(m_sockfd, NULL, NULL);
}

int socket::recv(char *buf, size_t len, int &oper_errno)
{
    if (m_ssl_instance && !m_ssl_accepted)
    {
        LOG_ERROR("socket m_ssl_instance but m_ssl_accepted is false, can not to recv");
        return -1;
    }

    // NO OPEN_SSL
    {
        if (!m_ssl_instance)
        {
            int result = ::recv(m_sockfd, buf, len, 0);
            if (result == -1)
            {
                oper_errno = errno;
            }
            return result;
        }
    }

    // USING OPENSSL
    {
        int bytes_received = SSL_read(m_ssl_instance, buf, len);
        if (bytes_received > 0)
        {
        }
        else if (bytes_received == 0)
        {
        }
        else
        {
            int ssl_error = SSL_get_error(m_ssl_instance, bytes_received);
            if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
            {
                oper_errno = EAGAIN;
            }
            else
            {
                oper_errno = errno;
            }
        }
        return bytes_received;
    }
}

int socket::send(const char *buf, size_t len, int &oper_errno)
{
    if (m_ssl_instance && !m_ssl_accepted)
    {
        LOG_ERROR("socket m_ssl_instance but m_ssl_accepted is false, can not to send");
        return -1;
    }
    // NO OPENSSL
    {
        // write data to m_sockfd
        if (!m_ssl_instance)
        {
            int result = ::send(m_sockfd, buf, len, 0);
            if (result == -1)
            {
                oper_errno = errno;
            }
            return result;
        }
    }
    // USING OPENSSL
    {
        int bytes_written = SSL_write(m_ssl_instance, buf, len);
        if (bytes_written > 0)
        {
        }
        else if (bytes_written == 0)
        {
        }
        else
        {
            int ssl_error = SSL_get_error(m_ssl_instance, bytes_written);
            if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
            {
                oper_errno = EAGAIN;
            }
            else
            {
                oper_errno = errno;
            }
        }
        return bytes_written;
    }
}

bool socket::set_non_blocking()
{
    int flags = fcntl(m_sockfd, F_GETFL, 0);
    if (flags < 0)
    {
        LOG_ERROR("socket::set_non_blocking(F_GETFL,O_NONBLOCK) errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    flags |= O_NONBLOCK; // setting nonblock
    if (fcntl(m_sockfd, F_SETFL, flags) < 0)
    {
        LOG_ERROR("socket::set_non_blocking(F_SETFL,O_NONBLOCK) errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::set_blocking()
{
    int flags = fcntl(m_sockfd, F_GETFL, 0);
    if (flags < 0)
    {
        LOG_ERROR("socket::set_blocking() errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    flags &= ~O_NONBLOCK; // setting nonblock
    if (fcntl(m_sockfd, F_SETFL, flags) < 0)
    {
        LOG_ERROR("socket::set_blocking() errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::set_send_buffer(size_t size)
{
    size_t buffer_size = size;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) < 0)
    {
        LOG_ERROR("socket set send buffer error: errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::set_recv_buffer(size_t size)
{
    int buffer_size = size;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0)
    {
        LOG_ERROR("socket set recv buffer errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::set_linger(bool active, size_t seconds)
{
    // l_onoff = 0; l_linger ignore
    // close()

    // l_onoff != 0;
    // l_linger = 0;
    // close()

    // l_onoff != 0;
    // l_linger > 0;
    // close()
    struct linger l;
    memset(&l, 0, sizeof(l));
    if (active)
        l.l_onoff = 1;
    else
        l.l_onoff = 0;
    l.l_linger = seconds;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) < 0)
    {
        LOG_ERROR("socket set linger error errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::set_keep_alive()
{
    int flag = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0)
    {
        LOG_ERROR("socket set sock keep alive error: errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::set_reuse_addr()
{
    int flag = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
    {
        LOG_ERROR("socket set sock reuser addr error: errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

bool socket::set_reuse_port()
{
    int flag = 1;
    if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) < 0)
    {
        LOG_ERROR("socket set sock reuser port error: errno=%d errstr=%s", errno, strerror(errno));
        return false;
    }
    return true;
}

int socket::create_tcp_socket(std::string ip)
{
    bool isIPV6 = is_ipv6(ip);
    int fd = -1;
    if (isIPV6)
    {
        fd = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    }
    else
    {
        fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }

    if (fd <= 0)
    {
        LOG_ERROR("create tcp socket error: errno=%d errstr=%s", errno, strerror(errno));
        return fd;
    }
    return fd;
}

bool socket::is_ipv6(std::string ip)
{
    if (std::string::npos != ip.find_first_of('.'))
    {
        return false;
    }
    return true;
}

int socket::get_fd()
{
    return this->m_sockfd;
}

SSL *socket::get_ssl_instance()
{
    return m_ssl_instance;
}

void socket::set_ssl_instance(SSL *ssl_instance)
{
    m_ssl_instance = ssl_instance;
}

bool socket::get_ssl_accepted()
{
    return m_ssl_accepted;
}

void socket::set_ssl_accepted(bool accepted)
{
    m_ssl_accepted = accepted;
}
