#include "socket/socket_pair.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <avant-log/logger.h>

using namespace avant::socket;

socket_pair::socket_pair()
{
}

socket_pair::~socket_pair()
{
}

int socket_pair::init()
{
    int iret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_fd);
    if (iret != 0)
    {
        return iret;
    }
    m_socket_obj[m_me_idx].set_fd(m_fd[m_me_idx]); // socket_obj auto ::close
    m_socket_obj[m_me_idx].set_non_blocking();
    m_socket_obj[m_me_idx].set_recv_buffer(65535);
    m_socket_obj[m_me_idx].set_send_buffer(65535);

    m_socket_obj[m_other_idx].set_fd(m_fd[m_other_idx]);
    m_socket_obj[m_other_idx].set_non_blocking();
    m_socket_obj[m_other_idx].set_recv_buffer(65535);
    m_socket_obj[m_other_idx].set_send_buffer(65535);
    return iret;
}

int socket_pair::get_me()
{
    return m_fd[m_me_idx];
}

int socket_pair::get_other()
{
    return m_fd[m_other_idx];
}

avant::socket::socket &socket_pair::get_me_socket()
{
    return std::ref(m_socket_obj[m_me_idx]);
}

avant::socket::socket &socket_pair::get_other_socket()
{
    return std::ref(m_socket_obj[m_other_idx]);
}