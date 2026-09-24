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

connection::connection()
    : fd(-1)
{
    this->recv_buffer.reserve(10 * 1024);
    this->send_buffer.reserve(10 * 1024);
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
    this->gid = 0;
    this->is_close = false;
    this->is_ready = false;
    this->traffic_sent_data_list.clear();
    this->traffic_recv_data_list.clear();
}

uint64_t connection::get_gid() const
{
    return this->gid;
}

avant::utility::vec_str_buffer &connection::get_recv_buffer()
{
    return this->recv_buffer;
}

avant::utility::vec_str_buffer &connection::get_send_buffer()
{
    return this->send_buffer;
}

int connection::get_fd()
{
    return this->fd;
}

void connection::set_fd(int fd)
{
    this->fd = fd;
}

avant::socket::socket &connection::get_socket_obj()
{
    return this->socket_obj;
}

std::shared_ptr<avant::connection::base_ctx> &connection::get_ctx_ptr()
{
    return this->ctx_ptr;
}

bool connection::get_closed_flag() const
{
    return this->closed_flag;
}

void connection::set_closed_flag(bool val)
{
    this->closed_flag = val;
}

bool connection::get_is_close() const
{
    return this->is_close;
}

void connection::set_is_close(bool val)
{
    this->is_close = val;
}

bool connection::get_is_ready() const
{
    return this->is_ready;
}

void connection::set_is_ready(bool val)
{
    this->is_ready = val;
}

void connection::record_traffic_bytes(std::deque<traffic_data> &traffic_data_list, size_t bytes)
{
    if (bytes == 0)
    {
        return;
    }

    if constexpr (false)
    {
        auto now = std::chrono::steady_clock::now();

        clear_timeout_traffic_data(traffic_data_list, now, traffic_data::max_traffic_data_seconds);

        if (traffic_data_list.empty())
        {
            traffic_data data(now, 0);
            traffic_data_list.push_back(data);
        }

        if (traffic_data_list.back().get_timestamp().time_since_epoch().count() != now.time_since_epoch().count())
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
    }
}

void connection::record_sent_bytes(size_t bytes)
{
    if (bytes == 0)
    {
        return;
    }
    record_traffic_bytes(this->traffic_sent_data_list, bytes);

    if constexpr (false)
    {
        std::chrono::time_point<std::chrono::steady_clock> timestamp_begin;
        std::chrono::time_point<std::chrono::steady_clock> timestamp_end;

        size_t bytes_total = 0;
        for (auto iter = this->traffic_sent_data_list.begin(); iter != this->traffic_sent_data_list.end();)
        {
            if (iter == this->traffic_sent_data_list.begin())
            {
                timestamp_begin = iter->get_timestamp();
            }

            if (iter == this->traffic_sent_data_list.end() - 1)
            {
                timestamp_end = iter->get_timestamp();
            }

            bytes_total += iter->bytes;

            ++iter;
        }

        std::cout << "record_sent_bytes: " << bytes_total << " bytes in " << std::chrono::duration_cast<std::chrono::seconds>(timestamp_end - timestamp_begin).count() << " seconds" << std::endl;
    }
}

void connection::record_recv_bytes(size_t bytes)
{
    if (bytes == 0)
    {
        return;
    }
    record_traffic_bytes(this->traffic_recv_data_list, bytes);

    if constexpr (false)
    {
        std::chrono::time_point<std::chrono::steady_clock> timestamp_begin;
        std::chrono::time_point<std::chrono::steady_clock> timestamp_end;

        size_t bytes_total = 0;
        for (auto iter = this->traffic_recv_data_list.begin(); iter != this->traffic_recv_data_list.end(); ++iter)
        {
            if (iter == this->traffic_recv_data_list.begin())
            {
                timestamp_begin = iter->get_timestamp();
            }

            if (iter == this->traffic_recv_data_list.end() - 1)
            {
                timestamp_end = iter->get_timestamp();
            }

            bytes_total += iter->bytes;
        }

        std::cout << "record_recv_bytes: " << bytes_total << " bytes in " << std::chrono::duration_cast<std::chrono::seconds>(timestamp_end - timestamp_begin).count() << " seconds" << std::endl;
    }
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
        if (now_timestamp - iter->get_timestamp() > std::chrono::seconds(seconds))
        {
            iter = traffic_data_list.erase(iter);
        }
        else
        {
            break;
        }
    }
}
