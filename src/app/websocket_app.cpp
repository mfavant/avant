#include "app/websocket_app.h"
#include "server/server.h"
#include "worker/worker.h"
#include <avant-log/logger.h>

using namespace avant::app;

void websocket_app::on_main_init(avant::server::server &server_obj)
{
}

void websocket_app::on_worker_init(avant::worker::worker &worker_obj)
{
}

void websocket_app::on_main_stop(avant::server::server &server_obj)
{
}

void websocket_app::on_worker_stop(avant::worker::worker &worker_obj)
{
}

void websocket_app::on_main_tick(avant::server::server &server_obj)
{
}

void websocket_app::on_worker_tick(avant::worker::worker &worker_obj)
{
}

void websocket_app::on_worker_tunnel(avant::worker::worker &worker_obj, const ProtoPackage &package)
{
    LOG_ERROR("websocket_app on_worker_tunnel cmd %d", package.cmd());
}
