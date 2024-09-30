#include "app/stream_app.h"
#include "server/server.h"
#include "workers/worker.h"
#include <avant-log/logger.h>
#include "proto_res/proto_example.pb.h"
#include "proto/proto_util.h"
#include "global/tunnel_id.h"
#include "utility/singleton.h"
#include "app/lua_plugin.h"

using namespace avant::app;
namespace utility = avant::utility;

void stream_app::on_main_init(avant::server::server &server_obj)
{
    LOG_ERROR("stream_app::on_main_init");
    utility::singleton<lua_plugin>::instance()->on_main_init(server_obj.get_lua_dir(), server_obj.get_worker_cnt());
}

void stream_app::on_worker_init(avant::workers::worker &worker_obj)
{
    LOG_ERROR("stream_app::on_worker_init %d", worker_obj.worker_id);
    utility::singleton<lua_plugin>::instance()->on_worker_init(worker_obj.worker_id, false);
}

void stream_app::on_main_stop(avant::server::server &server_obj)
{
    LOG_ERROR("stream_app::on_main_stop");
    utility::singleton<lua_plugin>::instance()->on_main_stop(false);
}

void stream_app::on_worker_stop(avant::workers::worker &worker_obj)
{
    LOG_ERROR("stream_app::on_worker_stop %d", worker_obj.worker_id);
    utility::singleton<lua_plugin>::instance()->on_worker_stop(worker_obj.worker_id, false);
}

void stream_app::on_main_tick(avant::server::server &server_obj)
{
    utility::singleton<lua_plugin>::instance()->on_main_tick();
}

void stream_app::on_worker_tick(avant::workers::worker &worker_obj)
{
    utility::singleton<lua_plugin>::instance()->on_worker_tick(worker_obj.worker_id);
}

bool stream_app::on_recved_packsize(avant::connection::stream_ctx &ctx, uint64_t size)
{
    return size <= 10240;
}

void stream_app::on_new_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_new_connection gid %llu", ctx.get_conn_gid());
}

void stream_app::on_close_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_close_connection gid %llu", ctx.get_conn_gid());
}

void stream_app::on_process_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_process_connection gid %llu", ctx.get_conn_gid());

    if (ctx.get_recv_buffer_size() > 2048000)
    {
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        LOG_ERROR("ctx.get_recv_buffer_size() > 2048000");
        return;
    }

    // parse protocol
    while (ctx.get_recv_buffer_size() > 0)
    {
        uint64_t data_size = 0;
        if (ctx.get_recv_buffer_size() >= sizeof(data_size))
        {
            data_size = *((uint64_t *)ctx.get_recv_buffer_read_ptr());
            data_size = avant::proto::toh64(data_size);

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
            LOG_ERROR("stream ctx client protoPackage.ParseFromArra failed %llu", data_size);
            ctx.recv_buffer_move_read_ptr_n(sizeof(data_size) + data_size);
            break;
        }

        ctx.recv_buffer_move_read_ptr_n(sizeof(data_size) + data_size);

        on_recv_package(ctx, protoPackage);
    }
}

void stream_app::on_recv_package(avant::connection::stream_ctx &ctx, const ProtoPackage &package)
{
    if (package.cmd() == ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE)
    {
        ProtoCSReqExample req;
        if (avant::proto::parse(req, package))
        {
            ProtoPackage resPackage;
            ProtoCSResExample res;
            res.set_testcontext(req.testcontext());

            // broadcast all connection in the process including this ctx self, async
            // ctx.worker_send_client_forward_message(ctx.get_conn_gid(), std::set<uint64_t>{}, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));

            // send_sync_package(ctx, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));

            ctx.worker_send_client_forward_message(ctx.get_conn_gid(), std::set<uint64_t>{ctx.get_conn_gid()}, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));
        }
    }
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
    if (cmd == ProtoCmd::PROTO_CMD_TUNNEL_OTHER2WORKER_TEST)
    {
        ProtoTunnelOther2WorkerTest other2worker_test;
        if (!proto::parse(other2worker_test, package))
        {
            LOG_ERROR("proto::parse(other2worker_test, package) failed");
            return;
        }
        // LOG_ERROR("worker_id %d PROTO_CMD_TUNNEL_OTHER2WORKER_TEST time %llu", worker_obj.worker_id, other2worker_test.time());
    }
    else
    {
        LOG_ERROR("not exist handler %d", cmd);
    }
}

// APP DEMO
void stream_app::on_client_forward_message(avant::connection::stream_ctx &ctx,
                                           bool self,
                                           ProtoTunnelClientForwardMessage &message,
                                           const ProtoTunnelPackage &tunnel_package)
{
    int cmd = message.innerprotopackage().cmd();
    if (cmd == ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE)
    {
        if (self)
        {
            stream_app::send_sync_package(ctx, message.innerprotopackage());
        }
    }
    else
    {
        LOG_ERROR("not exist handler %d", cmd);
    }
}

void stream_app::on_cmd_reload(avant::server::server &server_obj)
{
    LOG_ERROR("stream_app on_cmd_reload execute lua_plugin reload");
    utility::singleton<lua_plugin>::instance()->reload();
}