#include <cerrno>
#include <cstring>
#include <avant-log/logger.h>

#include "socket/client_socket.h"
#include "utility/singleton.h"

using namespace std;
using namespace avant::utility;
using namespace avant::socket;

client_socket::client_socket() : socket()
{
}

client_socket::client_socket(const string &ip, int port) : socket(ip, port)
{
    m_sockfd = socket::create_tcp_socket(ip);
    if (m_sockfd < 0)
    {
        LOG_ERROR("create client socket error: errno=%d errstr=%s", errno, strerror(errno));
        return;
    }
    set_non_blocking();
    set_recv_buffer(10 * 1024);
    set_send_buffer(10 * 1024);
    set_linger(true, 0);
    set_keep_alive();
    set_reuse_addr();
    set_reuse_port();
    if (!connect(ip, port))
    {
        LOG_ERROR("client socket connect ip %s port %d failed", ip.c_str(), port);
        return;
    }
}

client_socket::~client_socket()
{
    close();
}
