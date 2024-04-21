#include "connection/websocket_ctx.h"

using namespace avant::connection;

websocket_ctx::websocket_ctx()
{
}

websocket_ctx::~websocket_ctx()
{
}

void websocket_ctx::on_create(connection &conn_obj, avant::worker::worker &worker_obj)
{
    this->conn_ptr = &conn_obj;
    this->worker_ptr = &worker_obj;
}

void websocket_ctx::on_close()
{
    this->conn_ptr = nullptr;
    this->worker_ptr = nullptr;
}

void websocket_ctx::on_event(uint32_t event)
{
}