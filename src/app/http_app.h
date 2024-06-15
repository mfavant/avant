#pragma once
#include "connection/http_ctx.h"
#include "proto_res/proto_message_head.pb.h"

namespace avant
{
    namespace server
    {
        class server;
        class worker;
    }
    namespace app
    {
        class http_app
        {
        public:
            // ctx created
            static void on_ctx_create(avant::connection::http_ctx &ctx);

            // socket and ssl ready
            static void on_new_connection(avant::connection::http_ctx &ctx);

            static void process_connection(avant::connection::http_ctx &ctx);

            static int on_body(avant::connection::http_ctx &ctx, size_t length);

            static void on_main_init(avant::server::server &server_obj);

            static void on_worker_init(avant::workers::worker &worker_obj);

            static void on_main_stop(avant::server::server &server_obj);

            static void on_worker_stop(avant::workers::worker &worker_obj);

            static void on_main_tick(avant::server::server &server_obj);

            static void on_worker_tick(avant::workers::worker &worker_obj);

            static void on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package);
        };
    }
}