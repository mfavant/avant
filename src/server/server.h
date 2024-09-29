#pragma once
#include <string>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <avant-json/json.h>

#include "workers/worker.h"
#include "workers/other.h"
#include "task/task_type.h"
#include "event/event_poller.h"
#include "socket/socket_pair.h"
#include "connection/connection_mgr.h"
#include "socket/server_socket.h"
#include "utility/time.h"

#include "proto_res/proto_cmd.pb.h"
#include "proto_res/proto_message_head.pb.h"
#include "proto_res/proto_tunnel.pb.h"

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
                    size_t max_ipc_conn_num,
                    std::string ipc_json_path,
                    std::string crt_pem = "",
                    std::string key_pem = "",
                    bool use_ssl = false);
        avant::task::task_type get_task_type();
        void to_stop();
        inline int get_worker_cnt() const
        {
            return this->m_worker_cnt;
        }
        inline const avant::json::json &get_ipc_json()
        {
            return this->m_ipc_json;
        }

        void cmd_reload();

    private:
        bool on_stop();
        bool is_stop();
        void on_start();

        void on_start_load_ipc_json_file();

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
        void set_max_ipc_conn_num(size_t max_ipc_conn_num);
        void set_ipc_json_path(std::string ipc_json_path);

        void set_listen_info(const std::string &ip, int port);
        uint64_t server_gen_gid();

        void on_listen_event(std::vector<int> vec_new_client_fd, std::vector<uint64_t> vec_gid);
        void on_tunnel_event(avant::socket::socket_pair &tunnel, uint32_t event);
        void try_send_flush_tunnel(avant::socket::socket_pair &tunnel);

        // other-main-worker.. forward center
        void on_tunnel_process(ProtoPackage &message);
        int tunnel_forward(int source_tunnelid, int dest_tunnel_id, ProtoPackage &message, bool flush = true);

        connection::connection *get_main2worker_tunnel(int worker_tunnel_id);
        connection::connection *get_main2other_tunnel();

    private:
        std::string m_app_id{};
        std::string m_ip{};
        size_t m_port{0};
        size_t m_worker_cnt{0};
        size_t m_max_client_cnt{0};
        size_t m_epoll_wait_time{0};
        size_t m_accept_per_tick{0};
        avant::utility::time m_server_loop_time;
        uint64_t m_latest_tick_time{0};
        uint64_t m_gid_seq{0};

        std::string m_http_static_dir{};
        std::string m_lua_dir{};
        std::string m_task_type{};
        bool m_use_ssl{false};
        std::string m_crt_pem{};
        std::string m_key_pem{};
        size_t m_max_ipc_conn_num{0};
        std::string m_ipc_json_path{};
        avant::json::json m_ipc_json;

        volatile bool stop_flag{false};

        SSL_CTX *m_ssl_context{nullptr};
        std::shared_ptr<avant::socket::server_socket> m_server_listen_socket;
        avant::workers::worker *m_workers{nullptr};
        avant::workers::other *m_other{nullptr};

        std::shared_ptr<std::atomic<int>> m_curr_connection_num{nullptr};
        avant::socket::socket_pair *m_main_worker_tunnel{nullptr};
        std::unordered_map<int, int> m_main_worker_tunnel_fd2index;
        avant::socket::socket_pair m_main_other_tunnel;

        avant::event::event_poller m_epoller;
        avant::connection::connection_mgr m_main_connection_mgr;
    };
}
