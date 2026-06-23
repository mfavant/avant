/**
 * @file websocket_app.cpp
 * @author gaowanlu
 * @brief websocket app
 * @version 0.1
 * @date 2024-04-27
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "app/websocket_app.h"
#include "server/server.h"
#include "workers/worker.h"
#include <avant-log/logger.h>
#include "proto/proto_util.h"
#include "proto_res/proto_example.pb.h"
#include "proto_res/proto_cmd.pb.h"
#include "proto_res/proto_tunnel.pb.h"
#include "utility/singleton.h"
#include "app/lua_plugin.h"
#include <vector>
#include "global/tunnel_id.h"

using namespace avant::app;

void websocket_app::on_main_init(avant::server::server &server_obj)
{
    LOG_ERROR("websocket_app::on_main_init");
    utility::singleton<lua_plugin>::instance()->on_main_init(server_obj.get_config().get_lua_dir(),
                                                             server_obj.get_config().get_app_id(),
                                                             server_obj.get_config().get_worker_cnt());
}

void websocket_app::on_worker_init(avant::workers::worker &worker_obj)
{
    LOG_ERROR("websocket_app::on_worker_init {}", worker_obj.get_worker_idx());
    utility::singleton<lua_plugin>::instance()->on_worker_init(worker_obj.get_worker_idx());
}

void websocket_app::on_main_stop(avant::server::server &server_obj)
{
    LOG_ERROR("websocket_app::on_main_stop");
    utility::singleton<lua_plugin>::instance()->on_main_stop();
}

void websocket_app::on_worker_stop(avant::workers::worker &worker_obj)
{
    LOG_ERROR("websocket_app::on_worker_stop {}", worker_obj.get_worker_idx());
    utility::singleton<lua_plugin>::instance()->on_worker_stop(worker_obj.get_worker_idx());
}

void websocket_app::on_main_tick(avant::server::server &server_obj)
{
    utility::singleton<lua_plugin>::instance()->on_main_tick();
}

void websocket_app::on_worker_tick(avant::workers::worker &worker_obj)
{
    utility::singleton<lua_plugin>::instance()->on_worker_tick(worker_obj.get_worker_idx());
}

void websocket_app::on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package)
{
    int cmd = package.cmd();

    // Other 线程的Lua虚拟机发给客户端连接的包
    if (cmd == ProtoCmd::PROTO_CMD_TUNNEL_OTHERLUAVM2WORKERCONN)
    {
        ProtoTunnelOtherLuaVM2WorkerConn tunnelOtherLuaVM2WorkerConn;
        if (!proto::parse(tunnelOtherLuaVM2WorkerConn, package))
        {
            LOG_ERROR("proto::parse(tunnelOtherLuaVM2WorkerConn, package) failed");
            return;
        }
        uint64_t gid = tunnelOtherLuaVM2WorkerConn.gid();
        int worker_idx = tunnelOtherLuaVM2WorkerConn.workeridx();
        int cmd = tunnelOtherLuaVM2WorkerConn.innerprotopackage().cmd();

        if (cmd == ProtoCmd::PROTO_CMD_TUNNEL_OTHERLUAVM2WORKER_CLOSE_CLIENT_CONNECTION)
        {
            avant::connection::connection *close_conn = worker_obj.worker_connection_mgr->get_conn_by_gid(gid);
            if (close_conn)
            {
                auto close_websocket_ctx = dynamic_cast<avant::connection::websocket_ctx *>(close_conn->ctx_ptr.get());
                if (close_websocket_ctx)
                {
                    close_websocket_ctx->set_conn_is_close(true);
                    close_websocket_ctx->event_mod(nullptr, event::event_poller::RWE, false);
                }
            }
            return;
        }
        else
        {
            worker_obj.send_client_forward_message(gid, {gid}, *tunnelOtherLuaVM2WorkerConn.mutable_innerprotopackage());
        }

        // LOG_ERROR("stream_app::on_worker_tunnel gid {} worker_idx {} real_worker_idx {} cmd {}", gid, worker_idx, worker_obj.get_worker_id(), cmd);
    }
    else if (cmd == ProtoCmd::PROTO_CMD_TUNNEL_OTHER2WORKER_TEST)
    {
        LOG_ERROR("not exist handler {}", cmd);
    }
    else
    {
        LOG_ERROR("not exist handler {}", cmd);
    }
}

void websocket_app::on_new_connection(avant::connection::websocket_ctx &ctx)
{
    // LOG_ERROR("websocket_app::on_new_connection");
    ProtoTunnelWorker2OtherLuaVM worker2OtherLuaVMPkg;
    worker2OtherLuaVMPkg.set_gid(ctx.get_conn_gid());
    worker2OtherLuaVMPkg.set_workeridx(ctx.get_worker_idx());

    {
        ProtoPackage package;
        ProtoTunnelWorker2OtherEventNewClientConnection protoNewConn;
        protoNewConn.set_gid(ctx.get_conn_gid());
        avant::proto::pack_package(package, protoNewConn, ProtoCmd::PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_NEW_CLIENT_CONNECTION);
        *worker2OtherLuaVMPkg.mutable_innerprotopackage() = package;
    }

    ProtoPackage resPackage;
    ctx.tunnel_forward(
        std::vector{avant::global::tunnel_id::get().get_other_tunnel_id()},
        avant::proto::pack_package(resPackage, worker2OtherLuaVMPkg, ProtoCmd::PROOT_CMD_TUNNEL_WORKER2OTHER_LUAVM));
}

void websocket_app::on_close_connection(avant::connection::websocket_ctx &ctx)
{
    // LOG_ERROR("websocket_app::on_close_connection");
    ProtoTunnelWorker2OtherLuaVM worker2OtherLuaVMPkg;
    worker2OtherLuaVMPkg.set_gid(ctx.get_conn_gid());
    worker2OtherLuaVMPkg.set_workeridx(ctx.get_worker_idx());

    {
        ProtoPackage package;
        ProtoTunnelWorker2OtherEventCloseClientConnection protoCloseConn;
        protoCloseConn.set_gid(ctx.get_conn_gid());
        avant::proto::pack_package(package, protoCloseConn, ProtoCmd::PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_CLOSE_CLIENT_CONNECTION);
        *worker2OtherLuaVMPkg.mutable_innerprotopackage() = package;
    }

    ProtoPackage resPackage;
    ctx.tunnel_forward(
        std::vector{avant::global::tunnel_id::get().get_other_tunnel_id()},
        avant::proto::pack_package(resPackage, worker2OtherLuaVMPkg, ProtoCmd::PROOT_CMD_TUNNEL_WORKER2OTHER_LUAVM));
}

void websocket_app::on_process_connection(avant::connection::websocket_ctx &ctx)
{
    constexpr int max_package_num_per_loop = INT32_MAX;
    int package_num_per_loop = 0;
    do
    {
        uint64_t all_data_len = ctx.get_recv_buffer_size();
        if (all_data_len == 0)
        {
            break;
        }
        const char *data = ctx.get_recv_buffer_read_ptr();
        size_t index = 0;
        websocket_frame frame;

        {
            if (index >= all_data_len) // need next 1 byte
            {
                break;
            }
            uint8_t opcode = (uint8_t)data[index] & 0x0f;
            websocket_frame_type type = n_2_websocket_frame_type(opcode);

            if (type == websocket_frame_type::TEXT_FRAME || type == websocket_frame_type::BINARY_FRAME)
            {
                ctx.frame_first_opcode = opcode;
            }
            else if (type == websocket_frame_type::CONTINUATION_FRAME)
            {
            }
            else
            {
                LOG_ERROR("frame not be allowed. opcode = {}", opcode);
                ctx.set_conn_is_close(true);
                ctx.event_mod(nullptr, event::event_poller::RWE, false);
                break;
            }

            frame.fin = data[index] & 0x80;
            frame.opcode = data[index] & 0x0F;
            index++;
        }

        {
            if (index >= all_data_len) // need next 1 byte
            {
                // LOG_ERROR("index[{}] >= all_data_len[{}]", index, all_data_len);
                break;
            }
            frame.mask = (data[index] & 0x80) != 0;
            frame.payload_length = data[index] & 0x7F;
            index++;
        }

        {
            if (frame.payload_length == 126)
            {
                frame.payload_length = 0;
                if (index + 1 >= all_data_len) // need next 2 bytes
                {
                    // LOG_ERROR("index[{}] + 1 >= all_data_len[{}]", index, all_data_len);
                    break;
                }
                uint16_t tmp = 0;
                u_char *ph = nullptr;
                ph = (u_char *)&tmp;
                *ph++ = data[index];
                *ph++ = data[index + 1];
                tmp = ntohs(tmp);
                frame.payload_length = tmp;
                index += 2;
            }
            else if (frame.payload_length == 127)
            {
                frame.payload_length = 0;
                if (index + 7 >= all_data_len) // need next 8 bytes
                {
                    // LOG_ERROR("index[{}] + 7 >= all_data_len[{}]", index, all_data_len);
                    break;
                }
                uint32_t tmp = 0;
                u_char *ph = (u_char *)&tmp;
                *ph++ = data[index++]; // index+0
                *ph++ = data[index++]; // index+1
                *ph++ = data[index++]; // index+2
                *ph++ = data[index++]; // index+3
                frame.payload_length = ntohl(tmp);
                frame.payload_length = frame.payload_length << 32;
                ph = (u_char *)&tmp;
                *ph++ = data[index++]; // index+4
                *ph++ = data[index++]; // index+5
                *ph++ = data[index++]; // index+6
                *ph++ = data[index++]; // index+7
                tmp = ntohl(tmp);
                frame.payload_length = frame.payload_length | tmp;
            }
        }

        {
            if (frame.mask)
            {
                if (index + 3 >= all_data_len) // need next 4 bytes
                {
                    // LOG_ERROR("index[{}] + 3 >= all_data_len[{}]", index, all_data_len);
                    break;
                }
                frame.masking_key = {(uint8_t)data[index], (uint8_t)data[index + 1], (uint8_t)data[index + 2], (uint8_t)data[index + 3]};
                index += 4;
            }
        }

        {
            // already parser (value of the index) bytes
            if (index + frame.payload_length - 1 >= all_data_len) // need next frame.playload_length bytes
            {
                // LOG_ERROR("index[{}] + frame.payload_length[{}] - 1 >= all_data_len[{}]", index, frame.payload_length, all_data_len);
                break;
            }

            std::string payload_data;

            if (frame.payload_length != 0)
            {
                payload_data = std::string(&data[index], frame.payload_length);
            }

            if (frame.mask)
            {
                for (size_t i = 0; i < payload_data.size(); ++i)
                {
                    payload_data[i] ^= frame.masking_key[i % 4];
                }
            }

            frame.payload_data = std::move(payload_data);
            ctx.frame_payload_data += std::move(frame.payload_data);

            index += frame.payload_length;
        }

        {
            ctx.recv_buffer_move_read_ptr_n(index);
            if (frame.fin)
            {
                on_process_frame(ctx, frame);
                package_num_per_loop++;
                if (package_num_per_loop >= max_package_num_per_loop)
                {
                    break;
                }
            }
        }
    } while (true);

    if (ctx.get_recv_buffer_size() > 1024000)
    {
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        LOG_ERROR("ctx.get_recv_buffer_size() > 1024000");
        return;
    }

    if (ctx.frame_payload_data.size() > 1024000)
    {
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        LOG_ERROR("ctx.frame_payload_data.size() > 1024000");
        return;
    }
}

void websocket_app::on_process_frame(avant::connection::websocket_ctx &ctx, const websocket_frame &frame)
{
    // using tunnel to broadcast, 将此连接发的内容广播给其他所有连接
    // {
    //     ProtoTunnelWebsocketBroadcast websockBroadcast;
    //     ProtoPackage package;
    //     websockBroadcast.set_data(ctx.frame_payload_data);
    //     ctx.frame_payload_data.clear();
    //     ctx.worker_send_client_forward_message(ctx.get_conn_gid(), std::set<uint64_t>{}, avant::proto::pack_package(package, websockBroadcast, ProtoCmd::PROTO_CMD_TUNNEL_WEBSOCKET_BROADCAST));
    //     return;
    // }

    ProtoPackage protoPackage;
    if (!protoPackage.ParseFromArray(ctx.frame_payload_data.c_str(), ctx.frame_payload_data.size()))
    {
        LOG_ERROR("!protoPackage.ParseFromArray failed gid {}", ctx.get_conn_gid());
        ctx.frame_payload_data.clear();
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        return;
    }
    ctx.frame_payload_data.clear();

    // 将客户端连接发来的包 全部通过 ProtoTunnelWorker2OtherLuaVM 传给Other线程的Lua虚拟机
    ProtoTunnelWorker2OtherLuaVM worker2OtherLuaVMPkg;
    worker2OtherLuaVMPkg.set_gid(ctx.get_conn_gid());
    worker2OtherLuaVMPkg.set_workeridx(ctx.get_worker_idx());

    *worker2OtherLuaVMPkg.mutable_innerprotopackage() = protoPackage;
    ProtoPackage resPackage;
    ctx.tunnel_forward(
        std::vector{avant::global::tunnel_id::get().get_other_tunnel_id()},
        avant::proto::pack_package(resPackage, worker2OtherLuaVMPkg, ProtoCmd::PROOT_CMD_TUNNEL_WORKER2OTHER_LUAVM));
}

void websocket_app::on_client_forward_message(avant::connection::websocket_ctx &ctx,
                                              bool self,
                                              ProtoTunnelClientForwardMessage &message,
                                              const ProtoTunnelPackage &tunnel_package)
{
    int cmd = message.innerprotopackage().cmd();

    if (cmd == ProtoCmd::PROTO_CMD_TUNNEL_WEBSOCKET_BROADCAST)
    {
        uint8_t first_byte = 0x80 | websocket_frame_type_2_n(websocket_frame_type::TEXT_FRAME);
        ProtoTunnelWebsocketBroadcast websocketBroadcast;
        if (!avant::proto::parse(websocketBroadcast, message.innerprotopackage()))
        {
            LOG_ERROR("ProtoTunnelWebsocketBroadcast parse failed");
            return;
        }
        send_sync_package(ctx, first_byte, websocketBroadcast.data().c_str(), websocketBroadcast.data().size());
        return;
    }

    uint8_t first_byte = 0x80 | websocket_frame_type_2_n(websocket_frame_type::BINARY_FRAME);
    std::string data = message.innerprotopackage().SerializeAsString();
    send_sync_package(ctx, first_byte, data.c_str(), data.size());
}

int websocket_app::send_sync_package(avant::connection::websocket_ctx &ctx, uint8_t first_byte, const char *data, size_t data_len)
{
    if (ctx.get_send_buffer_size() > 1024000)
    {
        LOG_ERROR("ctx.get_send_buffer_size() > 1024000");
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        return -1;
    }

    size_t message_length = data_len;
    std::string frame;
    frame.push_back(first_byte);

    if (message_length <= 125)
    {
        frame.push_back(static_cast<uint8_t>(message_length));
    }
    else if (message_length <= 0xFFFF)
    {
        frame.push_back(126);
        frame.push_back((message_length >> 8) & 0xFF);
        frame.push_back(message_length & 0xFF);
    }
    else
    {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i)
        {
            frame.push_back((message_length >> (8 * i)) & 0xFF);
        }
    }
    frame.insert(frame.end(), data, data + data_len);
    return ctx.send_data(frame);
}

websocket_app::websocket_frame_type websocket_app::n_2_websocket_frame_type(uint8_t n)
{
    n = n & 0x0f;
    if (n == 0x1)
    {
        return websocket_frame_type::TEXT_FRAME;
    }
    else if (n == 0x2)
    {
        return websocket_frame_type::BINARY_FRAME;
    }
    else
    {
        if (n == 0x0)
        {
            return websocket_frame_type::CONTINUATION_FRAME;
        }
        else if (n >= 0x3 && n <= 0x7)
        {
            return websocket_frame_type::FURTHER_NON_CONTROL;
        }
        else if (n >= 0xb && n <= 0xf)
        {
            return websocket_frame_type::FURTHER_CONTROL;
        }
        else if (n == 0x8)
        {
            return websocket_frame_type::CONNECTION_CLOSE_FRAME;
        }
        else if (n == 0x9)
        {
            return websocket_frame_type::PING;
        }
        else if (n == 0xa)
        {
            return websocket_frame_type::PONG;
        }
    }
    return websocket_frame_type::ERROR;
}

uint8_t websocket_app::websocket_frame_type_2_n(websocket_frame_type type, uint8_t idx /*= 0x0*/)
{
    if (type == websocket_frame_type::TEXT_FRAME)
    {
        return 0x1;
    }
    else if (type == websocket_frame_type::BINARY_FRAME)
    {
        return 0x2;
    }
    else
    {
        if (type == websocket_frame_type::CONTINUATION_FRAME)
        {
            return 0x0;
        }
        else if (type == websocket_frame_type::FURTHER_NON_CONTROL && idx >= 0x0 && idx <= 0x4)
        {
            return 0x3 + idx;
        }
        else if (type == websocket_frame_type::FURTHER_CONTROL && idx >= 0x0 && idx <= 0x5)
        {
            return 0xb + idx;
        }
        else if (type == websocket_frame_type::CONNECTION_CLOSE_FRAME)
        {
            return 0x8;
        }
        else if (type == websocket_frame_type::PING)
        {
            return 0x9;
        }
        else if (type == websocket_frame_type::PONG)
        {
            return 0xa;
        }
    }
    return 0x8;
}

void websocket_app::on_cmd_reload(avant::server::server &server_obj)
{
    LOG_ERROR("websocket_app on_cmd_reload execute lua_plugin reload");
    utility::singleton<lua_plugin>::instance()->reload();
}
