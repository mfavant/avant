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
    for (int i = 0; i < m_fd_size; i++)
    {
        if (m_fd[i] > 0)
        {
            ::close(m_fd[i]);
        }
    }
}

int socket_pair::init()
{
    int iret = socketpair(AF_UNIX, SOCK_STREAM, 0, m_fd) < 0;
    return iret;
}

int socket_pair::get_me()
{
    return m_fd[0];
}

int socket_pair::get_other()
{
    return m_fd[1];
}