#include "connection/http_ctx.h"
#include <avant-log/logger.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include "app/http_app.h"
#include "server/server.h"
#include "utility/comm_errno.h"

using namespace avant::connection;

std::shared_ptr<llhttp_settings_t> http_ctx::settings;

void http_ctx::init_http_settings()
{
    if (http_ctx::settings)
    {
        return;
    }
    llhttp_settings_t *settings_ptr = new llhttp_settings_t;

    llhttp_settings_init(settings_ptr);

    http_ctx::settings.reset(settings_ptr);

    http_ctx::settings->on_message_begin = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_message_begin");
        return 0;
    };

    http_ctx::settings->on_protocol = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->protocol += std::string(at, length);
        if (t_http_ctx->protocol.size() > 10)
        {
            LOG_ERROR("http protocol.size() too big");
            return -1;
        }
        // LOG_ERROR("on_protocol");
        return 0;
    };

    http_ctx::settings->on_protocol_complete = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_protocol_complete");
        return 0;
    };

    http_ctx::settings->on_url = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->url += std::string(at, length);
        // LOG_ERROR("on_url {}", t_http_ctx->url.c_str());

        if (t_http_ctx->url.size() > 10240)
        {
            LOG_ERROR("http url.size() too big");
            return -1;
        }

        return 0;
    };

    http_ctx::settings->on_url_complete = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_url_complete");
        return 0;
    };

    http_ctx::settings->on_method = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->method += std::string(at, length);
        // LOG_ERROR("on_method {}", t_http_ctx->method.c_str());

        if (t_http_ctx->method.size() > 10)
        {
            LOG_ERROR("http method.size() too big");
            return -1;
        }

        return 0;
    };

    http_ctx::settings->on_method_complete = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_method_complete");
        return 0;
    };

    http_ctx::settings->on_version = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->version += std::string(at, length);
        if (t_http_ctx->version.size() > 10)
        {
            LOG_ERROR("http version.size() too big");
            return -1;
        }
        // LOG_ERROR("on_version");
        return 0;
    };

    http_ctx::settings->on_version_complete = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_version_complete");
        return 0;
    };

    http_ctx::settings->on_header_field = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->head_field_tmp += std::string(at, length);
        if (t_http_ctx->head_field_tmp.size() > 1024)
        {
            LOG_ERROR("http head_field_tmp.size() too big");
            return -1;
        }
        // LOG_ERROR("on_header_field {}", t_http_ctx->head_field_tmp.c_str());
        return 0;
    };

    http_ctx::settings->on_header_field_complete = [](llhttp_t *parser) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->head_value_tmp.clear();
        // LOG_ERROR("on_header_field_complete");
        return 0;
    };

    http_ctx::settings->on_header_value = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        std::string value(at, length);
        t_http_ctx->head_value_tmp += value;
        if (t_http_ctx->head_value_tmp.size() > 20480)
        {
            LOG_ERROR("http head_value_tmp.size() too big");
            return -1;
        }
        // LOG_ERROR("on_header_value");
        return 0;
    };

    http_ctx::settings->on_header_value_complete = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_header_value_complete");
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->add_header(t_http_ctx->head_field_tmp, t_http_ctx->head_value_tmp);
        t_http_ctx->head_field_tmp.clear();
        t_http_ctx->head_value_tmp.clear();
        return 0;
    };

    http_ctx::settings->on_headers_complete = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_headers_complete");
        return 0;
    };

    http_ctx::settings->on_chunk_header = [](llhttp_t *parser) -> auto
    {
        LOG_ERROR("http on_chunk_header not supported");
        return -1;
    };

    http_ctx::settings->on_chunk_complete = [](llhttp_t *parser) -> auto
    {
        LOG_ERROR("http on_chunk_complete not supported");
        return -1;
    };

    http_ctx::settings->on_chunk_extension_name = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        LOG_ERROR("http on_chunk_extension_name not supported");
        return -1;
    };

    http_ctx::settings->on_chunk_extension_name_complete = [](llhttp_t *parser) -> auto
    {
        LOG_ERROR("http on_chunk_extension_name_complete not supported");
        return -1;
    };

    http_ctx::settings->on_chunk_extension_value = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        LOG_ERROR("http on_chunk_extension_value not supported");
        return -1;
    };

    http_ctx::settings->on_chunk_extension_value_complete = [](llhttp_t *parser) -> auto
    {
        LOG_ERROR("http on_chunk_extension_value_complete not supported");
        return -1;
    };

    http_ctx::settings->on_body = [](llhttp_t *parser, const char *at, size_t length) -> auto
    {
        // LOG_ERROR("on_body length {}", length);
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        int iret = 0;

        try
        {
            iret = app::http_app::on_body_before(*t_http_ctx, at, length);
        }
        catch (const std::exception &e)
        {
            iret = -1;
            LOG_ERROR("{}", e.what());
        }

        if (iret == 0)
        {
            t_http_ctx->conn_ptr->recv_buffer.append(at, length);
            t_http_ctx->recv_body_size += length;
            try
            {
                iret = app::http_app::on_body(*t_http_ctx, length);
            }
            catch (const std::exception &e)
            {
                iret = -1;
                LOG_ERROR("{}", e.what());
            }
        }

        return iret;
    };

    http_ctx::settings->on_message_complete = [](llhttp_t *parser) -> auto
    {
        // LOG_ERROR("on_message_complete");
        http_ctx *t_http_ctx = static_cast<http_ctx *>(parser->data);
        t_http_ctx->set_recv_end(true);
        return 0;
    };
}

http_ctx::http_ctx()
{
}

http_ctx::~http_ctx()
{
}

void http_ctx::on_create(connection &conn_obj, avant::workers::worker &worker_obj, bool keep_alive)
{
    this->clear_app_layer_notified();
    this->conn_ptr = &conn_obj;
    this->worker_ptr = &worker_obj;
    this->url.clear();
    this->method.clear();
    this->protocol.clear();
    this->version.clear();
    this->headers.clear();
    this->head_field_tmp.clear();
    this->head_value_tmp.clear();
    this->process_callback = nullptr;
    this->write_end_callback = nullptr;
    this->destory_callback = nullptr;
    this->ptr = nullptr;
    llhttp_init(&this->http_parser_obj, HTTP_REQUEST, http_ctx::settings.get());
    this->http_parser_obj.data = this;
    this->recv_end = false;
    this->process_end = false;
    this->response_end = false;
    this->everything_end = false;
    this->keep_alive = keep_alive;
    this->recv_body_size = 0;
    if (this->keep_alive)
    {
        this->keep_live_counter++;
    }
    else
    {
        this->keep_live_counter = 0;
    }

    bool err = false;

    if (!err && (!this->worker_ptr->get_server()->get_config().get_use_ssl() || this->keep_alive))
    {
        this->conn_ptr->is_ready = true;
        try
        {
            this->set_app_layer_notified();
            this->worker_ptr->mark_delete_timeout_timer(this->get_conn_gid());
            app::http_app::on_new_connection(*this, keep_alive);
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
        this->worker_ptr->epoller.mod(conn_obj.fd, nullptr, event::event_poller::RWE, false);
        return;
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
        socket_ptr->close_callback = [socket_ptr]()
        {
            if (socket_ptr)
            {
                // LOG_DEBUG("http_ctx socket close_callback fd {}", socket_ptr->get_fd());
            }
        };
    }

    if (event & event::event_poller::ERR)
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
                LOG_ERROR("{}", e.what());
            }
        }
        this->worker_ptr->close_client_fd(socket_ptr->get_fd());
        return;
    }

    if (this->worker_ptr->get_server()->get_config().get_use_ssl() && !socket_ptr->get_ssl_accepted()) // ssl not ready
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
                app::http_app::on_new_connection(*this, false);
            }
            catch (const std::exception &e)
            {
                err = true;
                LOG_ERROR("{}", e.what());
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
                LOG_DEBUG("SSL_accept ssl_status[{}] error: {}", ssl_status, ERR_error_string(ERR_get_error(), nullptr));
                conn_ptr->is_close = true;
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                return;
            }
        }
    } // ssl end

    // read from socket
    if (!this->get_recv_end() && !this->get_everything_end() && (event & event::event_poller::READ))
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
            else if (len == -1 && oper_errno == avant::utility::comm_errno::comm_errno::COMM_ERRNO_EINTR)
            {
                len = 0;
                continue;
            }
            else if (len > 0)
            {
                this->conn_ptr->record_recv_bytes(len);
                llhttp_errno_t res_errno = llhttp_execute(&this->http_parser_obj, buffer + buffer_len, len);

                if (res_errno != llhttp_errno::HPE_OK)
                {
                    LOG_ERROR("res_errno {}", int(res_errno));
                    len = 0;
                    this->set_everything_end(true);
                    break;
                }
                else
                {
                    if (llhttp_get_upgrade(&this->http_parser_obj))
                    {
                        LOG_ERROR("http is upgrade");
                    }
                    buffer_len += len;
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
            LOG_ERROR("{}", e.what());
            this->set_everything_end(true);
        }
    }

    // write
    if (!this->get_everything_end() && this->get_process_end())
    {
        // from conn send buffer to socket
        while (!conn_ptr->send_buffer.empty() && (event & event::event_poller::WRITE))
        {
            int oper_errno = 0;
            int len = socket_ptr->send(conn_ptr->send_buffer.get_read_ptr(), conn_ptr->send_buffer.size(), oper_errno);
            if (0 > len)
            {
                if (oper_errno == avant::utility::comm_errno::comm_errno::COMM_ERRNO_EINTR)
                {
                    continue;
                }
                else if (oper_errno == avant::utility::comm_errno::comm_errno::COMM_ERRNO_EAGAIN ||
                         oper_errno == avant::utility::comm_errno::comm_errno::COMM_ERRNO_EWOULDBLOCK)
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
                conn_ptr->record_sent_bytes(len);
                conn_ptr->send_buffer.move_read_ptr_n(len);
            }
        }
        if (conn_ptr->send_buffer.empty() && !this->get_response_end())
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
                LOG_ERROR("{}", e.what());
                this->set_everything_end(true);
            }
            if (conn_ptr->send_buffer.empty() && this->get_response_end())
            {
                this->set_everything_end(true);
            }
        }
    }

    // response end and not exist bytes in send_buffer, then change to evething end
    if (conn_ptr->send_buffer.empty() && this->get_response_end())
    {
        this->set_everything_end(true);
    }

    // keep-live
    if (this->get_everything_end())
    {
        if (this->destory_callback)
        {
            try
            {
                this->destory_callback(*this);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("{}", e.what());
            }
            this->destory_callback = nullptr;
        }
        if (this->keep_alive)
        {
            // reuse connection
            this->conn_ptr->on_alloc(this->conn_ptr->fd, this->conn_ptr->gid);
            // reuse context
            this->on_create(*this->conn_ptr, *this->worker_ptr, true);
        }
    }

    // continue to epoll_wait
    if (!this->get_everything_end() && !this->get_recv_end()) // next loop for reading
    {
        this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RE, false);
        return;
    }

    if (!this->get_everything_end())
    {
        this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
        return;
    }

    this->set_everything_end(true);
    conn_ptr->is_close = true;
    this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
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

void http_ctx::send_buffer_append(const char *data, size_t len)
{
    this->conn_ptr->send_buffer.append(data, len);
}

size_t http_ctx::get_recv_buffer_size()
{
    return this->conn_ptr->recv_buffer.size();
}

void http_ctx::clear_recv_buffer()
{
    return this->conn_ptr->recv_buffer.clear();
}

uint64_t http_ctx::get_conn_gid()
{
    return this->conn_ptr->get_gid();
}

uint64_t http_ctx::get_recv_body_size()
{
    return this->recv_body_size;
}
