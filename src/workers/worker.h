#pragma once
#include <atomic>
#include <memory>
#include <string>
#include "event/event_poller.h"
#include "socket/socket_pair.h"
#include "connection/connection_mgr.h"

#include "proto_res/proto_cmd.pb.h"
#include "proto_res/proto_message_head.pb.h"
#include "proto_res/proto_tunnel.pb.h"
#include "task/task_type.h"
#include <avant-timer/timer_manager.h>
#include "utility/time.h"
#include <unordered_set>

namespace avant::workers
{
    class worker
    {
    public:
        worker();
        ~worker();
        void operator()();

        bool to_stop{false};
        bool is_stoped{false};

        int worker_id{-1};
        size_t max_client_num{0};
        bool use_ssl{false};
        size_t epoll_wait_time{0};

        SSL_CTX *ssl_context{nullptr};

        static std::string http_static_dir;

        avant::task::task_type type{avant::task::task_type::NONE};

        std::shared_ptr<std::atomic<int>> curr_connection_num{nullptr};

        avant::socket::socket_pair *main_worker_tunnel{nullptr};

        avant::event::event_poller epoller;
        std::shared_ptr<avant::connection::connection_mgr> worker_connection_mgr{nullptr};

    private:
        std::unordered_set<int> m_timeout_fd{};
        avant::timer::timer_manager m_conn_timeout_timer_manager;
        avant::utility::time m_worker_loop_time;
        uint64_t m_latest_tick_time{0};

    public:
        int tunnel_forward(const std::vector<int> &dest_tunnel_id, ProtoPackage &message, bool flush = true);
        int send_client_forward_message(uint64_t source_gid, const std::set<uint64_t> &dest_conn_gid, ProtoPackage &package);

        void close_client_fd(int fd);
        void mark_delete_timeout_timer(uint64_t timer_id);

    private:
        void try_send_flush_tunnel();

        void on_tunnel_event(uint32_t event);
        void on_client_event(int fd, uint32_t event);
        void on_tunnel_process(ProtoPackage &message);
        void on_new_client_fd(int fd, uint64_t gid);

        void handle_tunnel_client_forward_message(avant::connection::connection *conn_ptr, ProtoTunnelClientForwardMessage &message, const ProtoTunnelPackage &tunnel_package);
    };
}
