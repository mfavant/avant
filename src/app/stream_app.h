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
    }
    namespace app
    {
        class stream_app
        {
        public:
            static void on_main_init(avant::server::server &server_obj);

            static void on_worker_init(avant::workers::worker &worker_obj);

            static void on_main_stop(avant::server::server &server_obj);

            static void on_worker_stop(avant::workers::worker &worker_obj);

            static void on_main_tick(avant::server::server &server_obj);

            static void on_worker_tick(avant::workers::worker &worker_obj);

            static bool on_recved_packsize(avant::connection::stream_ctx &ctx, uint64_t size);

            // socket and ssl ready
            static void on_new_connection(avant::connection::stream_ctx &ctx);

            static void on_close_connection(avant::connection::stream_ctx &ctx);
            static void on_process_connection(avant::connection::stream_ctx &ctx);
            static void on_recv_package(avant::connection::stream_ctx &ctx, const ProtoPackage &package);
            static int send_sync_package(avant::connection::stream_ctx &ctx, const ProtoPackage &package);

            static void on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package);
            static void on_client_forward_message(avant::connection::stream_ctx &ctx,
                                                  bool self,
                                                  ProtoTunnelClientForwardMessage &message,
                                                  const ProtoTunnelPackage &tunnel_package);

            static void on_cmd_reload(avant::server::server &server_obj);
        };
    }
}