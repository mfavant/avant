#pragma once

#include "workers/other.h"
#include "connection/connection.h"
#include "connection/base_ctx.h"

namespace avant::connection
{
    class ipc_stream_ctx : public base_ctx
    {
    public:
        ipc_stream_ctx();
        ~ipc_stream_ctx();

        // context create success
        void on_create(connection &conn_obj, workers::other &other_obj);

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
            this->other_ptr->epoller.mod(this->conn_ptr->socket_obj.get_fd(), std::forward<Args>(args)...);
        }

        inline const workers::other *get_other_ptr()
        {
            return this->other_ptr;
        }

    protected:
        connection *conn_ptr{nullptr};
        workers::other *other_ptr{nullptr};
    };
}