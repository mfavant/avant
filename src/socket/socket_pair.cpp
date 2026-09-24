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
    int fd[M_FD_SIZE]{-1};
    int iret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if (iret != 0)
    {
        return iret;
    }
    m_socket_obj[M_ME_IDX].set_fd(fd[M_ME_IDX]); // socket_obj auto ::close
    if (!m_socket_obj[M_ME_IDX].set_non_blocking())
    {
        LOG_ERROR("socket_pair me endpoint set_non_blocking failed");
        m_socket_obj[M_ME_IDX].close();
        return -1;
    }
    if (!m_socket_obj[M_ME_IDX].set_recv_buffer(65535) || !m_socket_obj[M_ME_IDX].set_send_buffer(65535))
    {
        LOG_ERROR("socket_pair me endpoint set_recv_buffer/set_send_buffer failed");
        m_socket_obj[M_ME_IDX].close();
        return -1;
    }
    m_socket_obj[M_ME_IDX].close_callback = nullptr;

    m_socket_obj[M_OTHER_IDX].set_fd(fd[M_OTHER_IDX]);
    if (!m_socket_obj[M_OTHER_IDX].set_non_blocking())
    {
        LOG_ERROR("socket_pair other endpoint set_non_blocking failed");
        m_socket_obj[M_ME_IDX].close();
        m_socket_obj[M_OTHER_IDX].close();
        return -1;
    }
    if (!m_socket_obj[M_OTHER_IDX].set_recv_buffer(65535) || !m_socket_obj[M_OTHER_IDX].set_send_buffer(65535))
    {
        LOG_ERROR("socket_pair other endpoint set_recv_buffer/set_send_buffer failed");
        m_socket_obj[M_ME_IDX].close();
        m_socket_obj[M_OTHER_IDX].close();
        return -1;
    }
    m_socket_obj[M_OTHER_IDX].close_callback = nullptr;
    return 0;
}

int socket_pair::get_me()
{
    return m_socket_obj[M_ME_IDX].get_fd();
}

int socket_pair::get_other()
{
    return m_socket_obj[M_OTHER_IDX].get_fd();
}

avant::socket::socket &socket_pair::get_me_socket()
{
    return m_socket_obj[M_ME_IDX];
}

avant::socket::socket &socket_pair::get_other_socket()
{
    return m_socket_obj[M_OTHER_IDX];
}
