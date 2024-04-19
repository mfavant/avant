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
        LOG_ERROR("create server socket error: errno=%d errstr=%s", errno, strerror(errno));
        return;
    }
    set_non_blocking();
    set_recv_buffer(10 * 1024); // 10KB
    set_send_buffer(10 * 1024); // 10KB
    set_linger(true, 0);
    set_keep_alive();
    set_reuse_addr();
    set_reuse_port();
    bind(ip, port);
    listen(max_connections);
}

server_socket::~server_socket()
{
    close();
}
