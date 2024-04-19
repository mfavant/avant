#pragma once
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <memory>
#include <atomic>

#include "worker/worker.h"
#include "task/task_type.h"
#include "event/event_poller.h"

namespace avant::server
{
    class server
    {
    public:
        server();
        ~server();

        void start();
        inline size_t get_accept_per_tick() const
        {
            return m_accept_per_tick;
        }
        const std::string &get_http_static_dir();
        const std::string &get_lua_dir();
        bool get_use_ssl();
        std::string get_crt_pem();
        std::string get_key_pem();
        SSL_CTX *get_ssl_ctx();
        inline std::string get_app_id()
        {
            return m_app_id;
        }

        void config(const std::string &app_id,
                    const std::string &ip,
                    int port,
                    size_t worker_cnt,
                    size_t max_client_cnt,
                    size_t epoll_wait_time,
                    size_t accept_per_tick,
                    std::string task_type,
                    std::string http_static_dir,
                    std::string lua_dir,
                    std::string crt_pem = "",
                    std::string key_pem = "",
                    bool use_ssl = false);
        avant::task::task_type get_task_type();
        void to_stop();

    private:
        bool on_stop();
        bool is_stop();
        void on_start();

        void set_use_ssl(bool use_ssl);
        void set_crt_pem(std::string set_crt_pem);
        void set_key_pem(std::string set_key_pem);
        void set_lua_dir(std::string lua_dir);
        void set_task_type(std::string task_type);
        void set_http_static_dir(std::string http_static_dir);
        void set_worker_cnt(size_t worker_cnt);
        void set_max_client_cnt(size_t max_client_cnt);
        void set_epoll_wait_time(size_t epoll_wait_time);
        inline void set_accept_per_tick(size_t accept_per_tick)
        {
            m_accept_per_tick = accept_per_tick;
        }
        inline void set_app_id(std::string app_id)
        {
            m_app_id = app_id;
        }
        void set_listen_info(const std::string &ip, int port);

    private:
        std::string m_app_id{};
        std::string m_ip{};
        size_t m_port{0};
        size_t m_worker_cnt{0};
        size_t m_max_client_cnt{0};
        size_t m_epoll_wait_time{0};
        size_t m_accept_per_tick{0};

        std::string m_http_static_dir{};
        std::string m_lua_dir{};
        std::string m_task_type{};
        bool m_use_ssl{false};
        std::string m_crt_pem{};
        std::string m_key_pem{};
        volatile bool stop_flag{false};

        SSL_CTX *m_ssl_context{nullptr};
        std::shared_ptr<avant::worker::worker[]> m_workers{nullptr};
        std::atomic<int> m_curr_connection_num{0};

        avant::event::event_poller epoller;
    };
}
