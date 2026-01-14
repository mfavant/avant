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
#include "system/config_mgr.h"

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
        [[nodiscard]] SSL_CTX *get_ssl_ctx();

        void config(system::config_mgr *config_mgr);
        [[nodiscard]] const system::config_mgr *get_config() const { return m_config_mgr; }

        avant::task::task_type get_task_type();
        void to_stop();
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

        uint64_t server_gen_gid();

        void on_listen_event(std::vector<int> vec_new_client_fd, std::vector<uint64_t> vec_gid);
        void on_tunnel_event(avant::socket::socket_pair &tunnel, uint32_t event);
        void try_send_flush_tunnel(avant::socket::socket_pair &tunnel);

        // other-main-worker.. forward center
        void on_tunnel_process(ProtoPackage &message);
        int tunnel_forward(int source_tunnelid, int dest_tunnel_id, ProtoPackage &message, bool flush = true);

        [[nodiscard]] connection::connection *get_main2worker_tunnel(int worker_tunnel_id);
        [[nodiscard]] connection::connection *get_main2other_tunnel();

    private:
        system::config_mgr *m_config_mgr{nullptr};

        avant::utility::time m_server_loop_time;
        uint64_t m_latest_tick_time{0};
        uint64_t m_gid_seq{0};

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
