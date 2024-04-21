#pragma once

#include "worker/worker.h"
#include "connection/connection.h"

namespace avant::connection
{
    class stream_ctx
    {
    public:
        stream_ctx();
        ~stream_ctx();
        void on_create(connection &conn_obj, worker::worker &worker_obj);
        void on_close();
        void on_event(uint32_t event);
        connection *conn_ptr{nullptr};
        worker::worker *worker_ptr{nullptr};
    };
}