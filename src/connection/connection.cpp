#include "connection/connection.h"

using avant::connection::connection;

connection::connection()
{
    recv_buffer.reserve(10240);
    send_buffer.reserve(10240);
}

connection::~connection()
{
}

void connection::on_alloc()
{
    recv_buffer.clear();
    send_buffer.clear();
}

void connection::on_release()
{
    recv_buffer.clear();
    send_buffer.clear();
}