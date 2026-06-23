#pragma once
#include <cstdint>
#include "workers/other.h"
#include "connection/ipc_stream_ctx.h"

namespace avant
{
    namespace server
    {
        class server;
    }
    namespace app
    {
        class other_app
        {
        public:
            static void on_other_init(avant::workers::other &other_obj);
            static void on_other_stop(avant::workers::other &other_obj);
            static void on_other_tick(avant::workers::other &other_obj);
            static void on_other_tunnel(avant::workers::other &other_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package);

            static void on_new_connection_remote2this(avant::connection::ipc_stream_ctx &ctx);
            static void on_close_connection(avant::connection::ipc_stream_ctx &ctx);
            static void on_process_connection(avant::connection::ipc_stream_ctx &ctx);
            static void on_recv_package(avant::connection::ipc_stream_ctx &ctx, const ProtoPackage &package);

            static void other_lua_send_ipc_package(const std::string &app_id, int cmd, google::protobuf::Message &message);

            static void on_udp_server_recvfrom(avant::workers::other &other_obj, const char *buffer,
                                               ssize_t len,
                                               const struct sockaddr_storage &addr,
                                               socklen_t addr_len);
        };
    }
}