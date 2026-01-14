#include "connection/stream_ctx.h"
#include <avant-log/logger.h>
#include "app/stream_app.h"
#include "server/server.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include "utility/comm_errno.h"

using namespace avant::connection;

stream_ctx::stream_ctx()
{
}

stream_ctx::~stream_ctx()
{
}

void stream_ctx::on_create(connection &conn_obj, avant::workers::worker &worker_obj)
{
    this->clear_app_layer_notified();
    this->conn_ptr = &conn_obj;
    this->worker_ptr = &worker_obj;

    bool err = false;

    if (!err && !this->worker_ptr->get_server()->get_config()->get_use_ssl())
    {
        this->conn_ptr->is_ready = true;
        try
        {
            this->set_app_layer_notified();
            this->worker_ptr->mark_delete_timeout_timer(this->conn_ptr->get_gid());
            app::stream_app::on_new_connection(*this);
        }
        catch (const std::exception &e)
        {
            err = true;
            LOG_ERROR("{}", e.what());
        }
    }

    if (err)
    {
        this->conn_ptr->is_close = true;
        event_mod(nullptr, event::event_poller::RWE, false);
        return;
    }
}

void stream_ctx::on_close()
{
    this->conn_ptr = nullptr;
    this->worker_ptr = nullptr;
}

void stream_ctx::on_event(uint32_t event)
{
    avant::socket::socket *socket_ptr = &this->conn_ptr->socket_obj;
    avant::connection::connection *conn_ptr = this->conn_ptr;
    if (!socket_ptr->close_callback)
    {
        this->conn_ptr->socket_obj.close_callback = [socket_ptr]()
        {
            if (socket_ptr)
            {
                // LOG_DEBUG("stream_ctx socket close_callback fd {}", socket_ptr->get_fd());
            }
        };
    }

    if (event & event::event_poller::ERR)
    {
        conn_ptr->is_close = true;
    }

    if (conn_ptr->is_close)
    {
        if (conn_ptr->is_ready)
        {
            try
            {
                avant::app::stream_app::on_close_connection(*this);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("{}", e.what());
            }
        }

        this->worker_ptr->close_client_fd(socket_ptr->get_fd());
        return;
    }

    // ssl not ready, ssl begin
    if (this->worker_ptr->get_server()->get_config()->get_use_ssl() && !socket_ptr->get_ssl_accepted())
    {
        int ssl_status = SSL_accept(socket_ptr->get_ssl_instance());

        if (1 == ssl_status)
        {
            socket_ptr->set_ssl_accepted(true);
            conn_ptr->is_ready = true;
            // triger new connection hook
            bool err = false;
            try
            {
                this->set_app_layer_notified();
                this->worker_ptr->mark_delete_timeout_timer(this->conn_ptr->get_gid());
                app::stream_app::on_new_connection(*this);
            }
            catch (const std::exception &e)
            {
                err = true;
                LOG_ERROR("{}", e.what());
            }
            if (err)
            {
                conn_ptr->is_close = true;
                event_mod(nullptr, event::event_poller::RWE, false);
                return;
            }
        }
        else if (0 == ssl_status)
        {
            // need more data or space
            event_mod(nullptr, event::event_poller::RWE, false);
            return;
        }
        else
        {
            int ssl_error = SSL_get_error(socket_ptr->get_ssl_instance(), ssl_status);
            if (ssl_error == SSL_ERROR_WANT_READ)
            {
                // need more data or space
                event_mod(nullptr, event::event_poller::RE, false);
                return;
            }
            else if (ssl_error == SSL_ERROR_WANT_WRITE)
            {
                event_mod(nullptr, event::event_poller::RWE, false);
                return;
            }
            else
            {
                LOG_ERROR("SSL_accept ssl_status[{}] error: {}", ssl_status, ERR_error_string(ERR_get_error(), nullptr));
                conn_ptr->is_close = true;
                event_mod(nullptr, event::event_poller::RWE, false);
                return;
            }
        }
    } // ssl end

    // read from socket,parse protocol and process
    if (event & event::event_poller::READ)
    {
        int oper_errno = 0;
        int len = -1;
        constexpr int buffer_size = 10240;
        char buffer[buffer_size]{0};
        int buffer_len = 0;

        while (buffer_len < buffer_size)
        {
            len = socket_ptr->recv(buffer + buffer_len, buffer_size - buffer_len, oper_errno);
            if (len == -1 &&
                (oper_errno == avant::utility::comm_errno::comm_errno::COMM_ERRNO_EAGAIN ||
                 oper_errno == avant::utility::comm_errno::comm_errno::COMM_ERRNO_EWOULDBLOCK))
            {
                len = 0;
                break;
            }
            else if (len == -1 &&
                     oper_errno == avant::utility::comm_errno::comm_errno::COMM_ERRNO_EINTR)
            {
                len = 0;
                continue;
            }
            else if (len > 0)
            {
                buffer_len += len;
            }
            else
            {
                conn_ptr->is_close = true;
                event_mod(nullptr, event::event_poller::RWE, false);
                return;
            }
        }

        if (buffer_len > 0)
        {
            conn_ptr->record_recv_bytes(buffer_len);
            conn_ptr->recv_buffer.append(buffer, buffer_len);
        }
    }

    bool err = false;
    try
    {
        avant::app::stream_app::on_process_connection(*this);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("avant::app::stream_app::on_process_connection {}", e.what());
        err = true;
    }
    if (err)
    {
        conn_ptr->is_close = true;
        event_mod(nullptr, event::event_poller::RWE, false);
        return;
    }

    // write to socket
    while (event & event::event_poller::WRITE)
    {
        try_send_flush();
        break; // important
    }

    return;
}

void stream_ctx::try_send_flush()
{
    avant::socket::socket *socket_ptr = &this->conn_ptr->socket_obj;
    avant::connection::connection *conn_ptr = this->conn_ptr;
    if (conn_ptr->send_buffer.empty())
    {
        event_mod(nullptr, event::event_poller::RE, false);
        return;
    }
    while (!conn_ptr->send_buffer.empty())
    {
        int oper_errno = 0;
        int len = socket_ptr->send(conn_ptr->send_buffer.get_read_ptr(), conn_ptr->send_buffer.size(), oper_errno);
        if (len > 0)
        {
            conn_ptr->record_sent_bytes(len);
            conn_ptr->send_buffer.move_read_ptr_n(len);
        }
        else
        {
            if (oper_errno != avant::utility::comm_errno::comm_errno::COMM_ERRNO_EAGAIN &&
                oper_errno != avant::utility::comm_errno::comm_errno::COMM_ERRNO_EINTR &&
                oper_errno != avant::utility::comm_errno::comm_errno::COMM_ERRNO_EWOULDBLOCK)
            {
                // LOG_ERROR("stream ctx client sock send data oper_errno {}", oper_errno);
                conn_ptr->is_close = true;
            }
            event_mod(nullptr, event::event_poller::RWE, false);
            break;
        }
    }
}

int stream_ctx::send_data(const std::string &data, bool flush /*= true*/)
{
    if (this->conn_ptr->is_close || this->conn_ptr->closed_flag)
    {
        // LOG_ERROR("this->conn_ptr->is_close || this->conn_ptr->closed_flag forbiden send_data {}", this->conn_ptr->gid);
        return -1;
    }

    // need conn ready, forbiden to send_data
    if (!this->conn_ptr->is_ready)
    {
        // LOG_ERROR("!this->conn_ptr->is_ready {}", this->conn_ptr->gid);
        return -2;
    }

    this->conn_ptr->send_buffer.append(data.c_str(), data.size());
    if (flush)
    {
        try_send_flush();
    }
    else
    {
        event_mod(nullptr, event::event_poller::RWE, false);
    }
    return 0;
}

uint64_t stream_ctx::get_conn_gid()
{
    return this->conn_ptr->get_gid();
}

size_t stream_ctx::get_recv_buffer_size()
{
    return this->conn_ptr->recv_buffer.size();
}

const char *stream_ctx::get_recv_buffer_read_ptr()
{
    return this->conn_ptr->recv_buffer.get_read_ptr();
}

void stream_ctx::recv_buffer_move_read_ptr_n(size_t n)
{
    return this->conn_ptr->recv_buffer.move_read_ptr_n(n);
}

size_t stream_ctx::get_send_buffer_size()
{
    return this->conn_ptr->send_buffer.size();
}

void stream_ctx::set_conn_is_close(bool val)
{
    this->conn_ptr->is_close = val;
}
