#include "app/stream_app.h"
#include "server/server.h"
#include "workers/worker.h"
#include <avant-log/logger.h>
#include "proto_res/proto_example.pb.h"
#include "proto_res/proto_cmd.pb.h"
#include "proto_res/proto_tunnel.pb.h"
#include "proto/proto_util.h"
#include "global/tunnel_id.h"
#include "utility/singleton.h"
#include "app/lua_plugin.h"

using namespace avant::app;
namespace utility = avant::utility;

void stream_app::on_main_init(avant::server::server &server_obj)
{
    LOG_ERROR("stream_app::on_main_init");
    utility::singleton<lua_plugin>::instance()->on_main_init(server_obj.get_config().get_lua_dir(),
                                                             server_obj.get_config().get_app_id(),
                                                             server_obj.get_config().get_worker_cnt());
}

void stream_app::on_worker_init(avant::workers::worker &worker_obj)
{
    LOG_ERROR("stream_app::on_worker_init {}", worker_obj.get_worker_idx());
    utility::singleton<lua_plugin>::instance()->on_worker_init(worker_obj.get_worker_idx());
}

void stream_app::on_main_stop(avant::server::server &server_obj)
{
    LOG_ERROR("stream_app::on_main_stop");
    utility::singleton<lua_plugin>::instance()->on_main_stop();
}

void stream_app::on_worker_stop(avant::workers::worker &worker_obj)
{
    LOG_ERROR("stream_app::on_worker_stop {}", worker_obj.get_worker_idx());
    utility::singleton<lua_plugin>::instance()->on_worker_stop(worker_obj.get_worker_idx());
}

void stream_app::on_main_tick(avant::server::server &server_obj)
{
    utility::singleton<lua_plugin>::instance()->on_main_tick();
}

void stream_app::on_worker_tick(avant::workers::worker &worker_obj)
{
    utility::singleton<lua_plugin>::instance()->on_worker_tick(worker_obj.get_worker_idx());
}

bool stream_app::on_recved_packsize(avant::connection::stream_ctx &ctx, uint64_t size)
{
    return size <= 10240;
}

void stream_app::on_new_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_new_connection gid {}", ctx.get_conn_gid());
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

void stream_app::on_close_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_close_connection gid {}", ctx.get_conn_gid());
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

void stream_app::on_process_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_process_connection gid {}", ctx.get_conn_gid());

    if (ctx.get_recv_buffer_size() > 2048000)
    {
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        LOG_ERROR("ctx.get_recv_buffer_size() > 2048000");
        return;
    }

    // parse protocol
    constexpr int max_package_num_per_loop = INT32_MAX;
    int package_num_per_loop = 0;
    while (ctx.get_recv_buffer_size() > 0)
    {
        uint64_t data_size = 0;
        if (ctx.get_recv_buffer_size() >= sizeof(data_size))
        {
            data_size = avant::proto::toh64_from_buffer(ctx.get_recv_buffer_read_ptr());

            if (!avant::app::stream_app::on_recved_packsize(ctx, data_size))
            {
                ctx.set_conn_is_close(true);
                ctx.event_mod(nullptr, event::event_poller::RWE, false);
                return;
            }

            if (data_size + sizeof(data_size) > ctx.get_recv_buffer_size())
            {
                break;
            }
        }
        else
        {
            break;
        }

        if (data_size == 0)
        {
            ctx.recv_buffer_move_read_ptr_n(sizeof(data_size));
            break;
        }

        ProtoPackage protoPackage;
        if (!protoPackage.ParseFromArray(ctx.get_recv_buffer_read_ptr() + sizeof(data_size), data_size))
        {
            LOG_ERROR("stream ctx client protoPackage.ParseFromArra failed {}", data_size);
            ctx.recv_buffer_move_read_ptr_n(sizeof(data_size) + data_size);
            break;
        }

        ctx.recv_buffer_move_read_ptr_n(sizeof(data_size) + data_size);

        on_recv_package(ctx, protoPackage);
        package_num_per_loop++;
        if (package_num_per_loop >= max_package_num_per_loop)
        {
            break;
        }
    }
}

void stream_app::on_recv_package(avant::connection::stream_ctx &ctx, const ProtoPackage &package)
{
    // if (package.cmd() == ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE)
    // {
    //     ProtoCSReqExample req;
    //     if (avant::proto::parse(req, package))
    //     {
    //         ProtoPackage resPackage;
    //         ProtoCSResExample res;
    //         res.set_testcontext(req.testcontext());

    //         // broadcast all connection in the process including this ctx self, async
    //         // ctx.worker_send_client_forward_message(ctx.get_conn_gid(), std::set<uint64_t>{}, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));

    //         // send_sync_package(ctx, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));

    //         ctx.worker_send_client_forward_message(ctx.get_conn_gid(), std::set<uint64_t>{ctx.get_conn_gid()}, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));
    //     }
    //     return;
    // }

    ProtoTunnelWorker2OtherLuaVM worker2OtherLuaVMPkg;
    worker2OtherLuaVMPkg.set_gid(ctx.get_conn_gid());
    worker2OtherLuaVMPkg.set_workeridx(ctx.get_worker_idx());

    *worker2OtherLuaVMPkg.mutable_innerprotopackage() = package;
    ProtoPackage resPackage;
    ctx.tunnel_forward(
        std::vector{avant::global::tunnel_id::get().get_other_tunnel_id()},
        avant::proto::pack_package(resPackage, worker2OtherLuaVMPkg, ProtoCmd::PROOT_CMD_TUNNEL_WORKER2OTHER_LUAVM));
}

int stream_app::send_sync_package(avant::connection::stream_ctx &ctx, const ProtoPackage &package)
{
    if (ctx.get_send_buffer_size() > 1024000)
    {
        LOG_ERROR("ctx.get_send_buffer_size() > 1024000");
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        return -1;
    }

    std::string data;
    return ctx.send_data(avant::proto::pack_package(data, package));
}

void stream_app::on_worker_tunnel(avant::workers::worker &worker_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package)
{
    int cmd = package.cmd();
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
                auto close_stream_ctx = dynamic_cast<avant::connection::stream_ctx *>(close_conn->ctx_ptr.get());
                if (close_stream_ctx)
                {
                    close_stream_ctx->set_conn_is_close(true);
                    close_stream_ctx->event_mod(nullptr, event::event_poller::RWE, false);
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
        // ProtoTunnelOther2WorkerTest other2worker_test;
        // if (!proto::parse(other2worker_test, package))
        // {
        //     LOG_ERROR("proto::parse(other2worker_test, package) failed");
        //     return;
        // }
        // // LOG_ERROR("worker_id {} PROTO_CMD_TUNNEL_OTHER2WORKER_TEST time {}", worker_obj.get_worker_idx(), other2worker_test.time());
        // return;
    }
    else
    {
        LOG_ERROR("not exist handler {}", cmd);
    }
}

// APP DEMO
void stream_app::on_client_forward_message(avant::connection::stream_ctx &ctx,
                                           bool self,
                                           ProtoTunnelClientForwardMessage &message,
                                           const ProtoTunnelPackage &tunnel_package)
{
    int cmd = message.innerprotopackage().cmd();
    // if (cmd == ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE)
    // {
    //     if (self)
    //     {
    //         stream_app::send_sync_package(ctx, message.innerprotopackage());
    //     }
    //     return;
    // }

    stream_app::send_sync_package(ctx, message.innerprotopackage());
    // LOG_ERROR("on_client_forward_message cmd {}", cmd);
}

void stream_app::on_cmd_reload(avant::server::server &server_obj)
{
    LOG_ERROR("stream_app on_cmd_reload execute lua_plugin reload");
    utility::singleton<lua_plugin>::instance()->reload();
}