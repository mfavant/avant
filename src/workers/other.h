// other thread, support ipc
#pragma once
#include <iostream>
#include <memory>
#include "socket/socket_pair.h"
#include "event/event_poller.h"
#include "connection/connection_mgr.h"
#include "proto_res/proto_cmd.pb.h"
#include "proto_res/proto_message_head.pb.h"
#include "proto_res/proto_tunnel.pb.h"
#include <avant-json/json.h>
#include "socket/server_socket.h"
#include "utility/time.h"
#include <unordered_map>
#include <unordered_set>

namespace avant
{
    namespace server
    {
        class server;
    }
}

namespace avant::workers
{
    class other
    {
    public:
        other(avant::server::server *server = nullptr);
        ~other();
        void operator()();

        void set_server(avant::server::server *server) { m_server = server; }
        const avant::server::server *get_server() const { return m_server; }
        int init_call_by_server();

    public:
        int tunnel_forward(const std::vector<int> &dest_tunnel_id, ProtoPackage &message, bool flush = true);
        void close_ipc_client_fd(int fd);

        bool is_this2remote(uint64_t gid) const;
        bool is_remote2this(uint64_t gid) const;

    private:
        void try_send_flush_tunnel();
        void on_tunnel_event(uint32_t event);
        void on_ipc_listen_event(uint32_t event);
        void on_tunnel_process(ProtoPackage &message);

        uint64_t other_gen_gid();
        void on_new_ipc_client_fd(int fd, uint64_t gid);

        void on_ipc_client_event(int fd, uint32_t event);
        void ipc_client_to_connect();

    public:
        bool to_stop{false};
        bool is_stoped{false};

        avant::socket::socket_pair *main_other_tunnel{nullptr};
        avant::json::json ipc_json;
        avant::json::json ipc_self_json;

        std::shared_ptr<avant::socket::server_socket> ipc_listen_socket{nullptr};

        avant::event::event_poller epoller;
        std::shared_ptr<avant::connection::connection_mgr> ipc_connection_mgr{nullptr};

    private:
        avant::utility::time m_other_loop_time;
        uint64_t m_latest_tick_time{0};
        uint64_t m_gid_seq{0};

        // this process -> remote process, info mapping
        std::unordered_map<std::string, uint64_t> m_this2remote_appid2gid{};
        std::unordered_map<uint64_t, std::string> m_this2remote_gid2appid{};

        std::unordered_set<uint64_t> m_remote2this_gid{};

        avant::server::server *m_server;
    };
};
