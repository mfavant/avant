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

namespace avant::worker
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

        void close_client_fd(int fd);

    private:
        void on_tunnel_event(uint32_t event);
        void on_client_event(int fd, uint32_t event);
        int tunnel_forward(const std::vector<int> &dest_tunnel_id, ProtoPackage &message);
        void on_tunnel_process(ProtoPackage &message);
        void on_new_client_fd(int fd, uint64_t gid);

        void on_client_event_http(int fd, uint32_t event);
        void on_client_event_stream(int fd, uint32_t event);
        void on_client_event_websocket(int fd, uint32_t event);
    };
}
