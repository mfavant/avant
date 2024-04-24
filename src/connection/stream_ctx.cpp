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

    // TODO:
    // read from socket
    // parser protocol
    // process protocol
    // write to socket

    conn_ptr->is_close = true;
    this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
    return;
}