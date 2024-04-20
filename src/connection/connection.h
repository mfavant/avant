#pragma once
#include <cstdint>
#include "utility/vec_str_buffer.h"
#include "socket/socket.h"

namespace avant::connection
{
    class connection
    {
    public:
        connection();
        ~connection();
        avant::utility::vec_str_buffer recv_buffer;
        avant::utility::vec_str_buffer send_buffer;
        size_t recv_buffer_byte_idx{0};
        size_t send_buffer_byte_idx{0};
        int fd{0};
        uint64_t gid{0};
        avant::socket::socket socket_obj;
        void on_alloc();
        void on_release();
    };
};