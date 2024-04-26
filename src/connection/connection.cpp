#include "connection/connection.h"
#include "connection/http_ctx.h"
#include "connection/stream_ctx.h"
#include "connection/websocket_ctx.h"

using avant::connection::connection;

connection::connection()
{
    this->recv_buffer.reserve(10240);
    this->send_buffer.reserve(10240);
}

connection::~connection()
{
}

void connection::on_alloc(int fd, uint64_t gid)
{
    this->recv_buffer.clear();
    this->send_buffer.clear();
    this->fd = fd;
    this->gid = gid;
    this->closed_flag = false;
    this->is_close = false;
    this->is_ready = false;
}

void connection::on_release()
{
    if (this->http_ctx_ptr)
    {
        this->http_ctx_ptr->on_close();
    }
    if (this->stream_ctx_ptr)
    {
        this->stream_ctx_ptr->on_close();
    }
    if (this->websocket_ctx_ptr)
    {
        this->websocket_ctx_ptr->on_close();
    }
    this->recv_buffer.clear();
    this->send_buffer.clear();
    this->socket_obj.close();
    this->closed_flag = true;
    this->fd = -1;
    this->is_ready = false;
}
