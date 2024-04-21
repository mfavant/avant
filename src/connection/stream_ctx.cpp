#include "connection/stream_ctx.h"

using namespace avant::connection;

stream_ctx::stream_ctx()
{
}

stream_ctx::~stream_ctx()
{
}

void stream_ctx::on_create(connection &conn_obj, avant::worker::worker &worker_obj)
{
    this->conn_ptr = &conn_obj;
    this->worker_ptr = &worker_obj;
}

void stream_ctx::on_close()
{
    this->conn_ptr = nullptr;
    this->worker_ptr = nullptr;
}

void stream_ctx::on_event(uint32_t event)
{
}