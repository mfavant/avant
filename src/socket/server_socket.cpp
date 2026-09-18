#include <cerrno>
#include <cstring>
#include <avant-log/logger.h>

#include "socket/server_socket.h"
#include "socket/socket.h"
#include "utility/singleton.h"

using namespace std;
using namespace avant::socket;
using namespace avant::utility;

server_socket::server_socket() : socket()
{
}

server_socket::server_socket(const string &ip, int port, int max_connections) : socket(ip, port)
{
    m_sockfd = socket::create_tcp_socket(ip);
    if (m_sockfd < 0)
    {
        LOG_ERROR("create server socket error: errno={} errstr={}", errno, strerror(errno));
        return;
    }
    if (!set_non_blocking())
    {
        LOG_ERROR("server socket set_non_blocking failed, close fd {}", m_sockfd);
        close();
        return;
    }
    set_recv_buffer(10 * 1024); // 10KB
    set_send_buffer(10 * 1024); // 10KB
    set_linger(true, 0);
    set_keep_alive();
    set_reuse_addr();
    set_reuse_port();
    if (!bind(ip, port))
    {
        LOG_ERROR("server socket bind ip {} port {} failed, close fd {}", ip.c_str(), port, m_sockfd);
        close();
        return;
    }
    if (!listen(max_connections))
    {
        LOG_ERROR("server socket listen max_connections {} failed, close fd {}", max_connections, m_sockfd);
        close();
        return;
    }
}

server_socket::~server_socket()
{
    close();
}
