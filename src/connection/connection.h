#pragma once
#include "utility/vec_str_buffer.h"
#include "socket/socket.h"
#include <cstdint>
#include <memory>

namespace avant::connection
{
    class http_ctx;
    class stream_ctx;
    class websocket_ctx;

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
        void on_alloc(int fd);
        void on_release();
        std::shared_ptr<http_ctx> http_ctx_ptr{nullptr};
        std::shared_ptr<stream_ctx> stream_ctx_ptr{nullptr};
        std::shared_ptr<websocket_ctx> websocket_ctx_ptr{nullptr};
        bool closed_flag{false};
        bool is_close{false};
    };
}