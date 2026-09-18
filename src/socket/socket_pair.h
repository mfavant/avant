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
        static constexpr int M_ME_IDX{0};
        static constexpr int M_OTHER_IDX{1};
        static constexpr int M_FD_SIZE{2};
        avant::socket::socket m_socket_obj[M_FD_SIZE];
    };
};