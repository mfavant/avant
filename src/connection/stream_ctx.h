#pragma once

#include "workers/worker.h"
#include "connection/connection.h"
#include "connection/base_ctx.h"

namespace avant::connection
{
    class stream_ctx : public base_ctx
    {
    public:
        stream_ctx();
        ~stream_ctx();

        // context create success
        void on_create(connection &conn_obj, workers::worker &worker_obj);
        // context destory
        virtual void on_close() override;
        int send_data(const std::string &data, bool flush = true);

        void on_event(uint32_t event) override;

    private:
        void try_send_flush();

    public:
        uint64_t get_conn_gid();
        size_t get_recv_buffer_size();
        const char *get_recv_buffer_read_ptr();
        void recv_buffer_move_read_ptr_n(size_t n);

        size_t get_send_buffer_size();

        void set_conn_is_close(bool val);

        template <typename... Args>
        void event_mod(Args &&...args)
        {
            this->worker_ptr->epoller.mod(this->conn_ptr->socket_obj.get_fd(), std::forward<Args>(args)...);
        }

        template <typename... Args>
        int worker_send_client_forward_message(Args &&...args)
        {
            return this->worker_ptr->send_client_forward_message(std::forward<Args>(args)...);
        }

        template <typename... Args>
        int tunnel_forward(Args &&...args)
        {
            return this->worker_ptr->tunnel_forward(std::forward<Args>(args)...);
        }

        template <typename... Args>
        int get_worker_idx(Args &&...args)
        {
            return this->worker_ptr->get_worker_idx(std::forward<Args>(args)...);
        }

    protected:
        connection *conn_ptr{nullptr};
        workers::worker *worker_ptr{nullptr};
    };
}