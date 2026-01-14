#pragma once

#include "socket/socket.h"

namespace avant::socket
{
    class socket_pair
    {
    public:
        socket_pair();
        ~socket_pair();
        int init();

    public:
        int get_me();
        int get_other();
        [[nodiscard]]
        avant::socket::socket &get_me_socket();
        [[nodiscard]]
        avant::socket::socket &get_other_socket();

    private:
        static constexpr int m_me_idx{0};
        static constexpr int m_other_idx{1};
        static constexpr int m_fd_size{2};
        int m_fd[m_fd_size]{0};
        avant::socket::socket m_socket_obj[m_fd_size];
    };
};