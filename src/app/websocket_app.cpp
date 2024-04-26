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

void websocket_app::on_new_connection(avant::connection::websocket_ctx &ctx)
{
    // LOG_ERROR("websocket_app::on_new_connection");
}

void websocket_app::on_close_connection(avant::connection::websocket_ctx &ctx)
{
    // LOG_ERROR("websocket_app::on_close_connection");
}

void websocket_app::on_process_connection(avant::connection::websocket_ctx &ctx)
{
    connection::connection *conn_ptr = ctx.conn_ptr;
    socket::socket *socket_ptr = &ctx.conn_ptr->socket_obj;

    constexpr int max_recv_buffer = 1024000;
    if (conn_ptr->recv_buffer.size() > max_recv_buffer)
    {
        LOG_ERROR("conn_ptr max_recv_buffer content oversize %d", max_recv_buffer);
        conn_ptr->is_close = true;
        ctx.worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
        return;
    }
    // tmp clear
    conn_ptr->recv_buffer.clear();
    // LOG_ERROR("websocket_app::on_process_connection");
}