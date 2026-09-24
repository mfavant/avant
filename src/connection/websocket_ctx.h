#pragma once

#include "workers/worker.h"
#include "connection/connection.h"
#include <llhttp/llhttp.h>
#include <memory>
#include <string>
#include <map>
#include "connection/base_ctx.h"

namespace avant::connection
{
    class websocket_ctx : public base_ctx
    {
    public:
        websocket_ctx();
        ~websocket_ctx();

        // context create success
        void on_create(connection &conn_obj, workers::worker &worker_obj);
        // context destory
        virtual void on_close() noexcept override;

        void on_event(uint32_t event) noexcept override;

        void add_header(const std::string &key, const std::string &value);
        int send_data(const std::string &data, bool flush = true);

        virtual std::pair<std::string, int> get_ip_port() const override;

    private:
        void try_send_flush();

    public:
        static std::shared_ptr<llhttp_settings_t> settings;
        static void init_ws_http_settings();

    protected:
        connection *conn_ptr{nullptr};
        workers::worker *worker_ptr{nullptr};

    public:
        [[nodiscard]] uint64_t get_conn_gid() const;
        [[nodiscard]] size_t get_recv_buffer_size() const;
        [[nodiscard]] const char *get_recv_buffer_read_ptr() const;
        void recv_buffer_move_read_ptr_n(size_t n);

        [[nodiscard]] size_t get_send_buffer_size() const;

        void set_conn_is_close(bool val);

        template <typename... Args>
        void event_mod(Args &&...args)
        {
            if (this->conn_ptr == nullptr || this->worker_ptr == nullptr)
            {
                return;
            }
            this->worker_ptr->epoller.mod(this->conn_ptr->get_socket_obj().get_fd(), std::forward<Args>(args)...);
        }

        template <typename... Args>
        int worker_send_client_forward_message(Args &&...args)
        {
            if (this->worker_ptr == nullptr)
            {
                return -1;
            }
            return this->worker_ptr->send_client_forward_message(std::forward<Args>(args)...);
        }

        template <typename... Args>
        int tunnel_forward(Args &&...args)
        {
            if (this->worker_ptr == nullptr)
            {
                return -1;
            }
            return this->worker_ptr->tunnel_forward(std::forward<Args>(args)...);
        }

        template <typename... Args>
        int get_worker_idx(Args &&...args)
        {
            if (this->worker_ptr == nullptr)
            {
                return -1;
            }
            return this->worker_ptr->get_worker_idx(std::forward<Args>(args)...);
        }

    public:
        std::string url{};
        std::string method{};
        std::string protocol{};
        std::string version{};
        std::string sec_websocket_key{};
        std::string sec_websocket_version{};
        std::map<std::string, std::vector<std::string>> headers{};
        std::string head_field_tmp{};
        std::string head_value_tmp{};
        bool http_processed{false};
        bool is_upgrade{false};
        bool is_connected{false};

        uint8_t frame_first_opcode{0};
        std::string frame_payload_data{};

        void *ptr{nullptr};

    private:
        llhttp_t http_parser_obj;
    };
}