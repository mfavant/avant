#include "app/stream_app.h"
#include "server/server.h"
#include "worker/worker.h"
#include <avant-log/logger.h>
#include "proto_res/proto_example.pb.h"
#include "proto/proto_util.h"

using namespace avant::app;

void stream_app::on_main_init(avant::server::server &server_obj)
{
}

void stream_app::on_worker_init(avant::worker::worker &worker_obj)
{
}

void stream_app::on_main_stop(avant::server::server &server_obj)
{
}

void stream_app::on_worker_stop(avant::worker::worker &worker_obj)
{
}

void stream_app::on_main_tick(avant::server::server &server_obj)
{
}

void stream_app::on_worker_tick(avant::worker::worker &worker_obj)
{
}

bool stream_app::on_recved_packsize(avant::connection::stream_ctx &ctx, uint64_t size)
{
    return size <= 10240;
}

void stream_app::on_new_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_new_connection gid %llu", ctx.conn_ptr->gid);
}

void stream_app::on_close_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_close_connection gid %llu", ctx.conn_ptr->gid);
}

void stream_app::on_process_connection(avant::connection::stream_ctx &ctx, ProtoPackage &package)
{
    // LOG_ERROR("stream_app on_process_connection gid %llu", ctx.conn_ptr->gid);
    if (package.cmd() == ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE)
    {
        ProtoCSReqExample req;
        if (avant::proto::parse(req, package))
        {
            ProtoPackage resPackage;
            ProtoCSResExample res;
            res.set_testcontext(req.testcontext());
            send_sync_package(ctx, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));
        }
    }
}

int stream_app::send_sync_package(avant::connection::stream_ctx &ctx, ProtoPackage &package)
{
    return ctx.send_package(package);
}

// TODO:
// forward to main, main to worker
int stream_app::send_async_package(const std::unordered_set<uint64_t> &dest_conn_gid, ProtoPackage &package)
{
    return 0;
}

// TODO:
void stream_app::on_worker_tunnel(avant::worker::worker &worker_obj, const ProtoPackage &package)
{
    LOG_ERROR("stream_app on_worker_tunnel cmd %d", package.cmd());
}