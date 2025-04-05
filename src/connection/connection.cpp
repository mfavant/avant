#include "connection/connection.h"
#include "connection/http_ctx.h"
#include "connection/stream_ctx.h"
#include "connection/websocket_ctx.h"

using avant::connection::connection;

connection::traffic_data::traffic_data(const std::chrono::time_point<std::chrono::steady_clock> &timestamp,
                                       size_t bytes) : bytes(bytes),
                                                       timestamp(timestamp)
{
}

connection::traffic_data::~traffic_data()
{
}

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
    this->traffic_sent_data_list.clear();
    this->traffic_recv_data_list.clear();
}

void connection::on_release()
{
    if (this->ctx_ptr.get())
    {
        this->ctx_ptr->on_close();
    }

    this->recv_buffer.clear();
    this->send_buffer.clear();
    this->socket_obj.close();
    this->closed_flag = true;
    this->fd = -1;
    this->is_ready = false;
    this->traffic_sent_data_list.clear();
    this->traffic_recv_data_list.clear();
}

uint64_t connection::get_gid()
{
    return this->gid;
}

void connection::record_traffic_bytes(std::deque<traffic_data> &traffic_data_list, size_t bytes)
{
    if (bytes == 0)
    {
        return;
    }

#if 0
    auto now = std::chrono::steady_clock::now();

    clear_timeout_traffic_data(traffic_data_list, now, traffic_data::max_traffic_data_seconds);

    if (traffic_data_list.empty())
    {
        traffic_data data(now, 0);
        traffic_data_list.push_back(data);
    }

    if (traffic_data_list.back().timestamp.time_since_epoch().count() != now.time_since_epoch().count())
    {
        traffic_data data(now, 0);
        traffic_data_list.push_back(data);
    }

    while (bytes > 0)
    {
        size_t cap = std::numeric_limits<size_t>::max() - traffic_data_list.back().bytes;
        if (cap >= bytes)
        {
            traffic_data_list.back().bytes += bytes;
            bytes = 0;
        }
        else
        {
            traffic_data_list.back().bytes += cap;
            bytes -= cap;

            traffic_data data(now, 0);
            traffic_data_list.push_back(data);
        }
    }
#endif
}

void connection::record_sent_bytes(size_t bytes)
{
    if (bytes == 0)
    {
        return;
    }
    record_traffic_bytes(this->traffic_sent_data_list, bytes);

#if 0
    std::chrono::time_point<std::chrono::steady_clock> timestamp_begin;
    std::chrono::time_point<std::chrono::steady_clock> timestamp_end;

    size_t bytes_total = 0;
    for (auto iter = this->traffic_sent_data_list.begin(); iter != this->traffic_sent_data_list.end();)
    {
        if (iter == this->traffic_sent_data_list.begin())
        {
            timestamp_begin = iter->timestamp;
        }

        if (iter == this->traffic_sent_data_list.end() - 1)
        {
            timestamp_end = iter->timestamp;
        }

        bytes_total += iter->bytes;

        ++iter;
    }

    std::cout << "record_sent_bytes: " << bytes_total << " bytes in " << std::chrono::duration_cast<std::chrono::seconds>(timestamp_end - timestamp_begin).count() << " seconds" << std::endl;
#endif
}

void connection::record_recv_bytes(size_t bytes)
{
    if (bytes == 0)
    {
        return;
    }
    record_traffic_bytes(this->traffic_recv_data_list, bytes);

#if 0
    std::chrono::time_point<std::chrono::steady_clock> timestamp_begin;
    std::chrono::time_point<std::chrono::steady_clock> timestamp_end;

    size_t bytes_total = 0;
    for (auto iter = this->traffic_recv_data_list.begin(); iter != this->traffic_recv_data_list.end(); ++iter)
    {
        if (iter == this->traffic_recv_data_list.begin())
        {
            timestamp_begin = iter->timestamp;
        }

        if (iter == this->traffic_recv_data_list.end() - 1)
        {
            timestamp_end = iter->timestamp;
        }

        bytes_total += iter->bytes;
    }

    std::cout << "record_recv_bytes: " << bytes_total << " bytes in " << std::chrono::duration_cast<std::chrono::seconds>(timestamp_end - timestamp_begin).count() << " seconds" << std::endl;
#endif
}

void connection::clear_timeout_traffic_data(std::deque<traffic_data> &traffic_data_list,
                                            const std::chrono::time_point<std::chrono::steady_clock> &now_timestamp,
                                            size_t seconds)
{
    if (traffic_data_list.empty())
    {
        return;
    }
    auto iter = traffic_data_list.begin();
    while (iter != traffic_data_list.end())
    {
        if (now_timestamp - iter->timestamp > std::chrono::seconds(seconds))
        {
            iter = traffic_data_list.erase(iter);
        }
        else
        {
            break;
        }
    }
}
