#include "connection/stream_ctx.h"
#include <avant-log/logger.h>
#include "app/stream_app.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>

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
    if (!worker_obj.use_ssl)
    {
        bool err = false;
        try
        {
            app::stream_app::on_new_connection(*this);
        }
        catch (const std::exception &e)
        {
            err = true;
            LOG_ERROR(e.what());
        }
        if (err)
        {
            conn_ptr->is_close = true;
            this->worker_ptr->epoller.mod(conn_obj.fd, nullptr, event::event_poller::RWE, false);
        }
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
        this->conn_ptr->socket_obj.close_callback = [socket_ptr]() {
        };
    }

    if (event & event::event_poller::ERR)
    {
        conn_ptr->is_close = true;
    }

    if (conn_ptr->is_close)
    {
        if (!this->worker_ptr->use_ssl || socket_ptr->get_ssl_accepted())
        {
            try
            {
                avant::app::stream_app::on_close_connection(*this);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR(e.what());
            }
        }

        this->worker_ptr->close_client_fd(socket_ptr->get_fd());
        return;
    }

    // ssl begin
    if (this->worker_ptr->use_ssl && !socket_ptr->get_ssl_accepted())
    {
        int ssl_status = SSL_accept(socket_ptr->get_ssl_instance());

        if (1 == ssl_status)
        {
            socket_ptr->set_ssl_accepted(true);
            // triger new connection hook
            bool err = false;
            try
            {
                app::stream_app::on_new_connection(*this);
            }
            catch (const std::exception &e)
            {
                err = true;
                LOG_ERROR(e.what());
            }
            if (err)
            {
                conn_ptr->is_close = true;
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                return;
            }
        }
        else if (0 == ssl_status)
        {
            // need more data or space
            this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
            return;
        }
        else
        {
            int ssl_error = SSL_get_error(socket_ptr->get_ssl_instance(), ssl_status);
            if (ssl_error == SSL_ERROR_WANT_READ)
            {
                // need more data or space
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RE, false);
                return;
            }
            else if (ssl_error == SSL_ERROR_WANT_WRITE)
            {
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                return;
            }
            else
            {
                LOG_ERROR("SSL_accept ssl_status[%d] error: %s", ssl_status, ERR_error_string(ERR_get_error(), nullptr));
                conn_ptr->is_close = true;
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
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
            if (len == -1 && (oper_errno == EAGAIN || oper_errno == EWOULDBLOCK))
            {
                len = 0;
                break;
            }
            else if (len == -1 && oper_errno == EINTR)
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
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                return;
            }
        }

        if (buffer_len > 0)
        {
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
        LOG_ERROR("avant::app::stream_app::on_process_connection %s", e.what());
        err = true;
    }
    if (err)
    {
        conn_ptr->is_close = true;
        this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
        return;
    }

    // write to socket
    while (event & event::event_poller::WRITE)
    {
        if (conn_ptr->send_buffer.empty())
        {
            this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RE, false);
            break;
        }

        while (!conn_ptr->send_buffer.empty())
        {
            int oper_errno = 0;
            int len = socket_ptr->send(conn_ptr->send_buffer.get_read_ptr(), conn_ptr->send_buffer.size(), oper_errno);
            if (len > 0)
            {
                conn_ptr->send_buffer.move_read_ptr_n(len);
            }
            else
            {
                if (oper_errno != EAGAIN && oper_errno != EINTR && oper_errno != EWOULDBLOCK)
                {
                    LOG_ERROR("stream ctx client sock send data oper_errno %d", oper_errno);
                    conn_ptr->is_close = true;
                    this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                    return;
                }
                break;
            }
        }
        break; // important
    }

    return;
}

int stream_ctx::send_data(const std::string &data)
{
    this->conn_ptr->send_buffer.append(data.c_str(), data.size());
    this->worker_ptr->epoller.mod(this->conn_ptr->fd, nullptr, event::event_poller::RWE, false);
    return 0;
}