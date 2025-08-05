#pragma once

#include "workers/worker.h"
#include "connection/connection.h"
#include <http-parser/http_parser.h>
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
        virtual void on_close() override;

        void on_event(uint32_t event) override;

        const workers::worker *get_worker_ptr() const { return this->worker_ptr; }

        void set_recv_end(bool recv_end);
        bool get_recv_end();
        void set_process_end(bool process_end);
        bool get_process_end();
        void set_response_end(bool response_end);
        bool get_response_end();
        void set_everything_end(bool everything_end);
        bool get_everything_end();

        void add_header(const std::string &key, const std::string &value);

        void send_buffer_append(const char *data, size_t len);
        size_t get_recv_buffer_size();
        void clear_recv_buffer();
        uint64_t get_conn_gid();
        uint64_t get_recv_body_size();

        template <typename... Args>
        int tunnel_forward(Args &&...args)
        {
            return this->worker_ptr->tunnel_forward(std::forward<Args>(args)...);
        }

    public:
        static std::shared_ptr<http_parser_settings> settings;
        static void init_http_settings();

    protected:
        connection *conn_ptr{nullptr};
        workers::worker *worker_ptr{nullptr};

    public:
        std::string url{};
        std::string method{};
        std::map<std::string, std::vector<std::string>> headers{};
        std::string head_field_tmp{};
        std::function<void(http_ctx &ctx)> process_callback{nullptr};
        std::function<void(http_ctx &ctx)> write_end_callback{nullptr};
        std::function<void(http_ctx &ctx)> destory_callback{nullptr};
        void *ptr{nullptr};
        bool keep_alive{false};

    private:
        http_parser http_parser_obj;
        bool recv_end{false};
        bool process_end{false};
        bool response_end{false};
        bool everything_end{false};

        uint64_t keep_live_counter{0};
        uint64_t recv_body_size{0};
    };
}