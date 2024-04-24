#include "app/stream_app.h"
#include "server/server.h"
#include "worker/worker.h"
#include <avant-log/logger.h>

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

void stream_app::on_new_connection(avant::connection::stream_ctx &ctx)
{
}

void stream_app::on_close_connection(avant::connection::stream_ctx &ctx)
{
}

void stream_app::on_process_connection(avant::connection::stream_ctx &ctx, ProtoPackage &package)
{
}

// forward to main
int stream_app::send_async_package(const std::unordered_set<uint64_t> &dest_conn_gid, ProtoPackage &package)
{
    return 0;
}

int stream_app::send_sync_package(avant::connection::connection &dest_conn, ProtoPackage &package)
{
    return 0;
}

void stream_app::on_worker_tunnel(avant::worker::worker &worker_obj, const ProtoPackage &package)
{
    LOG_ERROR("stream_app on_worker_tunnel cmd %d", package.cmd());
}