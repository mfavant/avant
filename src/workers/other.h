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

namespace avant::workers
{
    class other
    {
    public:
        other();
        ~other();
        void operator()();

    public:
        int tunnel_forward(const std::vector<int> &dest_tunnel_id, ProtoPackage &message, bool flush = true);

    private:
        void try_send_flush_tunnel();
        void on_tunnel_event(uint32_t event);
        void on_tunnel_process(ProtoPackage &message);

    public:
        bool to_stop{false};
        bool is_stoped{false};

        size_t max_ipc_conn_num{0};
        size_t epoll_wait_time{0};

        avant::socket::socket_pair *main_other_tunnel{nullptr};

        avant::event::event_poller epoller;
        std::shared_ptr<avant::connection::connection_mgr> ipc_connection_mgr{nullptr};
    };
};
