#pragma once
#include "utility/vec_str_buffer.h"
#include "socket/socket.h"
#include <cstdint>
#include <memory>

namespace avant::connection
{
    class base_ctx;

    class connection
    {
    public:
        connection();
        ~connection();
        avant::utility::vec_str_buffer recv_buffer;
        avant::utility::vec_str_buffer send_buffer;
        int fd{0};
        uint64_t gid{0};
        avant::socket::socket socket_obj;
        void on_alloc(int fd, uint64_t gid);
        void on_release();
        uint64_t get_gid();
        std::shared_ptr<base_ctx> ctx_ptr{nullptr};
        bool closed_flag{false};
        bool is_close{false};
        bool is_ready{false}; // will be setting true on notify app new_connection
    };
}