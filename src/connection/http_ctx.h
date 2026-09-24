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
    class http_ctx : public base_ctx
    {
    public:
        http_ctx();
        ~http_ctx();

        // context create success
        void on_create(connection &conn_obj, workers::worker &worker_obj, bool keep_alive);
        // context destory
        virtual void on_close() noexcept override;

        void on_event(uint32_t event) noexcept override;

        const workers::worker *get_worker_ptr() const { return this->worker_ptr; }

        void set_recv_end(bool recv_end);
        [[nodiscard]] bool get_recv_end() const;
        void set_process_end(bool process_end);
        [[nodiscard]] bool get_process_end() const;
        void set_response_end(bool response_end);
        [[nodiscard]] bool get_response_end() const;
        void set_everything_end(bool everything_end);
        [[nodiscard]] bool get_everything_end() const;

        void add_header(const std::string &key, const std::string &value);

        void send_buffer_append(const char *data, size_t len);
        [[nodiscard]] size_t get_recv_buffer_size() const;
        void clear_recv_buffer();
        [[nodiscard]] uint64_t get_conn_gid() const;
        [[nodiscard]] uint64_t get_recv_body_size() const;

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

        virtual std::pair<std::string, int> get_ip_port() const override;

    public:
        static std::shared_ptr<llhttp_settings_t> settings;
        static void init_http_settings();

    protected:
        connection *conn_ptr{nullptr};
        workers::worker *worker_ptr{nullptr};

    public:
        std::string url{};
        std::string method{};
        std::string protocol{};
        std::string version{};
        std::map<std::string, std::vector<std::string>> headers{};
        std::string head_field_tmp{};
        std::string head_value_tmp{};
        std::function<void(http_ctx &ctx)> process_callback{nullptr};
        std::function<void(http_ctx &ctx)> write_end_callback{nullptr};
        std::function<void(http_ctx &ctx)> destory_callback{nullptr};
        void *ptr{nullptr};
        bool keep_alive{false};

    private:
        llhttp_t http_parser_obj;
        bool recv_end{false};
        bool process_end{false};
        bool response_end{false};
        bool everything_end{false};

        uint64_t recv_body_size{0};
    };
}