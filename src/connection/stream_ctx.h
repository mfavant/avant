#pragma once

#include "workers/worker.h"
#include "connection/connection.h"

namespace avant::connection
{
    class stream_ctx
    {
    public:
        stream_ctx();
        ~stream_ctx();

        // context create success
        void on_create(connection &conn_obj, workers::worker &worker_obj);
        // context destory
        void on_close();
        int send_data(const std::string &data);

        void on_event(uint32_t event);
        connection *conn_ptr{nullptr};
        workers::worker *worker_ptr{nullptr};
    };
}