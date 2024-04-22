#include "connection/http_ctx.h"
#include <avant-log/logger.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include "app/http_app.h"

using namespace avant::connection;

std::shared_ptr<http_parser_settings> http_ctx::settings;

void http_ctx::init_http_settings()
{
    if (http_ctx::settings)
    {
        return;
    }
    auto settings_ptr = new http_parser_settings;
    http_ctx::settings.reset(settings_ptr);

    http_ctx::settings->on_message_begin = [](http_parser *parser) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        http_method method = (http_method)parser->method;
        t_http_ctx->method = http_method_str(method);
        return 0;
    };

    http_ctx::settings->on_url = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->url = std::string(at, length);
        return 0;
    };

    http_ctx::settings->on_status = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        return 0;
    };

    http_ctx::settings->on_header_field = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->head_field_tmp = std::string(at, length);
        return 0;
    };

    http_ctx::settings->on_header_value = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        std::string value(at, length);
        t_http_ctx->add_header(t_http_ctx->head_field_tmp, value);
        return 0;
    };

    http_ctx::settings->on_headers_complete = [](http_parser *parser) -> auto
    {
        return 0;
    };

    http_ctx::settings->on_body = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->conn_ptr->recv_buffer.append(at, length);
        int iret = 0;
        try
        {
            iret = app::http_app::on_body(*t_http_ctx);
        }
        catch (const std::exception &e)
        {
            iret = -1;
            LOG_ERROR(e.what());
        }

        return iret;
    };

    http_ctx::settings->on_message_complete = [](http_parser *parser) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->set_recv_end(true);
        return 0;
    };

    http_ctx::settings->on_chunk_header = [](http_parser *parser) -> auto
    {
        return 0;
    };

    http_ctx::settings->on_chunk_complete = [](http_parser *parser) -> auto
    {
        return 0;
    };
}

http_ctx::http_ctx()
{
}

http_ctx::~http_ctx()
{
}

void http_ctx::on_create(connection &conn_obj, avant::worker::worker &worker_obj)
{
    this->conn_ptr = &conn_obj;
    this->worker_ptr = &worker_obj;
    this->url.clear();
    this->method.clear();
    this->headers.clear();
    this->head_field_tmp.clear();
    this->process_callback = nullptr;
    this->write_end_callback = nullptr;
    this->destory_callback = nullptr;
    this->ptr = nullptr;
    http_parser_init(&this->http_parser_obj, HTTP_REQUEST);
    this->http_parser_obj.data = this;
    this->recv_end = false;
    this->process_end = false;
    this->response_end = false;
    this->everything_end = false;

    if (!worker_obj.use_ssl)
    {
        app::http_app::on_new_connection(*this);
    }
}

void http_ctx::on_close()
{
    this->conn_ptr = nullptr;
    this->worker_ptr = nullptr;
}

void http_ctx::on_event(uint32_t event)
{
    socket::socket *socket_ptr = &this->conn_ptr->socket_obj;
    avant::connection::connection *conn_ptr = this->conn_ptr;
    if (!socket_ptr->close_callback)
    {
        this->conn_ptr->socket_obj.close_callback = [socket_ptr]() {
        };
    }

    if (event & (EPOLLHUP | EPOLLRDHUP | EPOLLERR))
    {
        conn_ptr->is_close = true;
    }

    if (conn_ptr->is_close)
    {
        if (this->destory_callback)
        {
            try
            {
                this->destory_callback(*this);
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
            app::http_app::on_new_connection(*this);
        }
        else if (0 == ssl_status)
        {
            // need more data or space
            this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
            return;
        }
        else
        {
            int ssl_error = SSL_get_error(socket_ptr->get_ssl_instance(), ssl_status);
            if (ssl_error == SSL_ERROR_WANT_READ)
            {
                // need more data or space
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
                return;
            }
            else if (ssl_error == SSL_ERROR_WANT_WRITE)
            {
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
                return;
            }
            else
            {
                LOG_ERROR("SSL_accept ssl_status[%d] error: %s", ssl_status, ERR_error_string(ERR_get_error(), nullptr));
                conn_ptr->is_close = true;
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
                return;
            }
        }
    } // ssl end

    // ==========================================TEST MAX HTTP QPS NOT USE HTTP-PARSER BEGIN==========================================
    {
        // this->set_recv_end(true);
    }
    // ==========================================TEST MAX HTTP QPS NOT USE HTTP-PARSER END==========================================

    // read from socket
    if (!this->get_recv_end() && !this->get_everything_end() && (event & EPOLLIN))
    {
        int oper_errno = 0;
        int len = -1;
        constexpr int buffer_size = 1024;
        char buffer[buffer_size]{0};
        while (true)
        {
            len = socket_ptr->recv(buffer, buffer_size, oper_errno);
            if (len == -1 && oper_errno == EAGAIN)
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
                int nparsed = http_parser_execute(&this->http_parser_obj, http_ctx::settings.get(), buffer, len);
                if (this->http_parser_obj.upgrade)
                {
                }
                else if (nparsed != len) // err
                {
                    LOG_ERROR("nparsed != len");
                    len = 0;
                    this->set_everything_end(true);
                    break;
                }
            }
            else
            {
                this->set_everything_end(true);
                break;
            }
        }
    }

    // process http connection
    if (!this->get_everything_end() && this->get_recv_end() && !this->get_process_end())
    {
        this->set_process_end(true);
        try
        {
            app::http_app::process_connection(*this);
            if (this->process_callback)
            {
                this->process_callback(*this);
            }
            else
            {
                this->set_everything_end(true);
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR(e.what());
            this->set_everything_end(true);
        }
    }

    // write
    if (!this->get_everything_end() && this->get_process_end())
    {
        // from conn send buffer to socket
        while (!conn_ptr->send_buffer.empty() && (event & EPOLLOUT))
        {
            int oper_errno = 0;
            int len = socket_ptr->send(conn_ptr->send_buffer.get_read_ptr(), conn_ptr->send_buffer.size(), oper_errno);
            if (0 > len)
            {
                if (oper_errno == EINTR)
                {
                    continue;
                }
                else if (oper_errno == EAGAIN)
                {
                    break;
                }
                else // error
                {
                    this->set_response_end(true);
                    this->set_everything_end(true);
                    break;
                }
            }
            else if (0 == len) // disconnect or nothing to send
            {
                this->set_everything_end(true);
                break;
            }
            else
            {
                conn_ptr->send_buffer.move_read_ptr_n(len);
            }
        }
        if (conn_ptr->send_buffer.empty())
        {
            try
            {
                if (this->write_end_callback)
                {
                    this->write_end_callback(*this);
                }
                else
                {
                    this->set_everything_end(true);
                }
            }
            catch (const std::exception &e)
            {
                LOG_ERROR(e.what());
                this->set_everything_end(true);
            }
            if (conn_ptr->send_buffer.empty() && this->get_response_end())
            {
                this->set_everything_end(true);
            }
        }
    }

    // continue to epoll_wait
    if (!this->get_everything_end() && !this->get_recv_end()) // next loop for reading
    {
        this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
        return;
    }

    if (!this->get_everything_end())
    {
        this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
        return;
    }

    this->set_everything_end(true);
    conn_ptr->is_close = true;
    this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
}

void http_ctx::set_recv_end(bool recv_end)
{
    this->recv_end = recv_end;
}

bool http_ctx::get_recv_end()
{
    return this->recv_end;
}

void http_ctx::set_process_end(bool process_end)
{
    this->process_end = process_end;
}

bool http_ctx::get_process_end()
{
    return this->process_end;
}

void http_ctx::set_response_end(bool response_end)
{
    this->response_end = response_end;
}

bool http_ctx::get_response_end()
{
    return this->response_end;
}

void http_ctx::set_everything_end(bool everything_end)
{
    this->everything_end = everything_end;
}

bool http_ctx::get_everything_end()
{
    return this->everything_end;
}

void http_ctx::add_header(const std::string &key, const std::string &value)
{
    auto res = this->headers.find(key);
    if (res == this->headers.end())
    {
        std::vector<std::string> m_vec;
        this->headers[key] = m_vec;
    }
    this->headers[key].push_back(value);
}
