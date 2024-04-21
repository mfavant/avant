#pragma once
#include "utility/vec_str_buffer.h"
#include "socket/socket.h"
#include <cstdint>
#include <memory>
#include "connection/http_ctx.h"
#include "connection/stream_ctx.h"
#include "connection/websocket_ctx.h"

namespace avant::connection
{
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
        void on_alloc();
        void on_release();
        std::shared_ptr<avant::connection::http_ctx> http_ctx_ptr{nullptr};
        std::shared_ptr<avant::connection::stream_ctx> stream_ctx_ptr{nullptr};
        std::shared_ptr<avant::connection::websocket_ctx> websocket_ctx_ptr{nullptr};
    };
};