#pragma once

#include "connection/websocket_ctx.h"
#include <cstdint>

namespace avant
{
    namespace server
    {
        class server;
    }
    namespace app
    {
        class websocket_app
        {
        public:
            struct websocket_frame
            {
                uint8_t fin;
                uint8_t opcode;
                uint8_t mask;
                uint64_t payload_length;
                std::vector<uint8_t> masking_key;
                std::string payload_data;
            };
            enum class websocket_frame_type
            {
                CONNECTION_CLOSE_FRAME = 0,
                TEXT_FRAME = 1,
                BINARY_FRAME = 2,
                PONG = 3,
                PING = 4,
                CONTINUATION_FRAME = 5,
                FURTHER_NON_CONTROL = 6,
                FURTHER_CONTROL = 7,
                ERROR = 8
            };
            static websocket_frame_type n_2_websocket_frame_type(uint8_t n);
            static uint8_t websocket_frame_type_2_n(websocket_frame_type type, uint8_t idx = 0x0);

        public:
            static void on_main_init(avant::server::server &server_obj);

            static void on_worker_init(avant::workers::worker &worker_obj);

            static void on_main_stop(avant::server::server &server_obj);

            static void on_worker_stop(avant::workers::worker &worker_obj);

            static void on_main_tick(avant::server::server &server_obj);

            static void on_worker_tick(avant::workers::worker &worker_obj);

            static void on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package);

            // socket and ssl and websocket handshake ready
            static void on_new_connection(avant::connection::websocket_ctx &ctx);

            static void on_close_connection(avant::connection::websocket_ctx &ctx);
            static void on_process_connection(avant::connection::websocket_ctx &ctx);

            static void on_process_frame(avant::connection::websocket_ctx &ctx, const websocket_frame &frame);
            static void on_client_forward_message(avant::connection::websocket_ctx &ctx,
                                                  bool self,
                                                  ProtoTunnelClientForwardMessage &message,
                                                  const ProtoTunnelPackage &tunnel_package);
            static int send_sync_package(avant::connection::websocket_ctx &ctx, uint8_t first_byte, const char *data, size_t data_len);

            static void on_cmd_reload(avant::server::server &server_obj);
        };
    }
}