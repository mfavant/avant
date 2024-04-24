#include "connection/connection.h"
#include "connection/http_ctx.h"
#include "connection/stream_ctx.h"
#include "connection/websocket_ctx.h"

using avant::connection::connection;

connection::connection()
{
    recv_buffer.reserve(10240);
    send_buffer.reserve(10240);
}

connection::~connection()
{
}

void connection::on_alloc(int fd, uint64_t gid)
{
    recv_buffer.clear();
    send_buffer.clear();
    this->fd = fd;
    this->gid = gid;
    closed_flag = false;
    is_close = false;
}

void connection::on_release()
{
    if (http_ctx_ptr)
    {
        http_ctx_ptr->on_close();
    }
    if (stream_ctx_ptr)
    {
        stream_ctx_ptr->on_close();
    }
    if (websocket_ctx_ptr)
    {
        websocket_ctx_ptr->on_close();
    }
    recv_buffer.clear();
    send_buffer.clear();
    socket_obj.close();
    closed_flag = true;
    fd = -1;
}
