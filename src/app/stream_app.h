#pragma once
#include "connection/stream_ctx.h"
#include "proto_res/proto_message_head.pb.h"
#include <unordered_set>
#include <cstdint>

namespace avant
{
    namespace server
    {
        class server;
        class worker;
    }
    namespace app
    {
        class stream_app
        {
        public:
            static void on_main_init(avant::server::server &server_obj);

            static void on_worker_init(avant::worker::worker &worker_obj);

            static void on_main_stop(avant::server::server &server_obj);

            static void on_worker_stop(avant::worker::worker &worker_obj);

            static void on_main_tick(avant::server::server &server_obj);

            static void on_worker_tick(avant::worker::worker &worker_obj);

            static bool on_recved_packsize(avant::connection::stream_ctx &ctx, uint64_t size);

            static void on_new_connection(avant::connection::stream_ctx &ctx);
            static void on_close_connection(avant::connection::stream_ctx &ctx);
            static void on_process_connection(avant::connection::stream_ctx &ctx);
            static int send_async_package(const std::unordered_set<uint64_t> &dest_conn_gid, ProtoPackage &package);
            static int send_sync_package(avant::connection::stream_ctx &ctx, ProtoPackage &package);

            static void on_worker_tunnel(avant::worker::worker &worker_obj, const ProtoPackage &package);
        };
    }
}