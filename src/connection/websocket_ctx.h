#pragma once

#include "worker/worker.h"
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
        void on_create(connection &conn_obj, worker::worker &worker_obj);
        // context destory
        void on_close();

        void on_event(uint32_t event);

        void add_header(const std::string &key, const std::string &value);

    public:
        connection *conn_ptr{nullptr};
        worker::worker *worker_ptr{nullptr};
        static std::shared_ptr<http_parser_settings> settings;
        static void init_ws_http_settings();

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
        void *ptr{nullptr};

    private:
        http_parser http_parser_obj;
    };
}