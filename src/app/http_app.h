#pragma once
#include "connection/http_ctx.h"
#include "proto_res/proto_message_head.pb.h"

namespace avant
{
    namespace server
    {
        class server;
    }
    namespace app
    {
        class http_app
        {
        public:
            /**
             * @brief TCP socket and SSL ready
             *
             * @param ctx
             * @param is_keep_alive_call Is it triggered due to the reuse of the keep_alive connection
             */
            static void on_new_connection(avant::connection::http_ctx &ctx, bool is_keep_alive_call);

            static void process_connection(avant::connection::http_ctx &ctx);

            // Prior to appending the HTTP body data uploaded by the client to the recv_buffer, an advance notification is issued
            static int on_body_before(avant::connection::http_ctx &ctx, const char *at, size_t length);

            // The client's uploaded HTTP body data has just been appended to the recv_buffer
            static int on_body(avant::connection::http_ctx &ctx, size_t length);

            static void on_main_init(avant::server::server &server_obj);

            static void on_worker_init(avant::workers::worker &worker_obj);

            static void on_main_stop(avant::server::server &server_obj);

            static void on_worker_stop(avant::workers::worker &worker_obj);

            static void on_main_tick(avant::server::server &server_obj);

            static void on_worker_tick(avant::workers::worker &worker_obj);

            static void on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package);

            static void on_cmd_reload(avant::server::server &server_obj);
        };
    }
}