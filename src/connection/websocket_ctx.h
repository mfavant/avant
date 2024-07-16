#pragma once

#include "workers/worker.h"
#include "connection/connection.h"
#include <memory>
#include <http-parser/http_parser.h>
#include <memory>
#include <string>
#include <map>

namespace avant::connection
{
    class websocket_ctx
    {
    public:
        websocket_ctx();
        ~websocket_ctx();

        // context create success
        void on_create(connection &conn_obj, workers::worker &worker_obj);
        // context destory
        void on_close();

        void on_event(uint32_t event);

        void add_header(const std::string &key, const std::string &value);
        int send_data(const std::string &data, bool flush = true);

    private:
        void try_send_flush();

    public:
        static std::shared_ptr<http_parser_settings> settings;
        static void init_ws_http_settings();

    protected:
        connection *conn_ptr{nullptr};
        workers::worker *worker_ptr{nullptr};

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

    public:
        std::string url{};
        std::string method{};
        std::string sec_websocket_key{};
        std::string sec_websocket_version{};
        std::map<std::string, std::vector<std::string>> headers{};
        std::string head_field_tmp{};
        bool http_processed{false};
        bool everything_end{false};
        bool is_upgrade{false};
        bool is_connected{false};

        uint8_t frame_first_opcode{0};
        std::string frame_payload_data{};

        void *ptr{nullptr};

    private:
        http_parser http_parser_obj;
    };
}