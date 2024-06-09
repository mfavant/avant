#include "connection/websocket_ctx.h"
#include "app/websocket_app.h"
#include <stdexcept>
#include <avant-log/logger.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include "utility/sha1.h"
#include "utility/base64.h"

using namespace avant::connection;

std::shared_ptr<http_parser_settings> websocket_ctx::settings;

void websocket_ctx::init_ws_http_settings()
{
    if (websocket_ctx::settings)
    {
        return;
    }
    websocket_ctx::settings.reset(new http_parser_settings);

    settings->on_message_begin = [](http_parser *parser) -> auto
    {
        websocket_ctx *ctx = static_cast<websocket_ctx *>(parser->data);
        http_method method = (http_method)parser->method;
        ctx->method = http_method_str(method);
        if (ctx->method != "GET")
        {
            return -1;
        }
        return 0;
    };

    settings->on_url = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        websocket_ctx *ctx = static_cast<websocket_ctx *>(parser->data);
        ctx->url = std::string(at, length);
        return 0;
    };

    settings->on_status = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        return 0;
    };

    settings->on_header_field = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        websocket_ctx *ctx = static_cast<websocket_ctx *>(parser->data);
        ctx->head_field_tmp = std::string(at, length);
        return 0;
    };

    settings->on_header_value = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        websocket_ctx *ctx = static_cast<websocket_ctx *>(parser->data);
        std::string value(at, length);
        ctx->add_header(ctx->head_field_tmp, value);
        return 0;
    };

    settings->on_headers_complete = [](http_parser *parser) -> auto
    {
        // websocket_ctx *ctx = static_cast<websocket_ctx *>(parser->data);
        return 0;
    };

    settings->on_body = [](http_parser *parser, const char *at, size_t length) -> auto
    {
        if (length > 0)
        {
            return -1;
        }
        return 0;
    };

    settings->on_message_complete = [](http_parser *parser) -> auto
    {
        websocket_ctx *ctx = static_cast<websocket_ctx *>(parser->data);
        ctx->http_processed = true;
        return 0;
    };

    settings->on_chunk_header = [](http_parser *parser) -> auto
    {
        return -1;
    };

    settings->on_chunk_complete = [](http_parser *parser) -> auto
    {
        return -1;
    };
}

websocket_ctx::websocket_ctx()
{
}

websocket_ctx::~websocket_ctx()
{
}

// socket fd be created
void websocket_ctx::on_create(connection &conn_obj, avant::workers::worker &worker_obj)
{
    this->conn_ptr = &conn_obj;
    this->worker_ptr = &worker_obj;
    this->url.clear();
    this->method.clear();
    this->sec_websocket_key.clear();
    this->sec_websocket_version.clear();
    this->headers.clear();
    this->head_field_tmp.clear();
    this->http_processed = false;
    this->everything_end = false;
    this->is_upgrade = false;
    this->is_connected = false;
    this->frame_first_opcode = 0;
    this->frame_payload_data.clear();
    this->ptr = nullptr;
    http_parser_init(&this->http_parser_obj, HTTP_REQUEST);
    this->http_parser_obj.data = this;

    bool err = false;

    // notify app layer
    {
        try
        {
            app::websocket_app::on_ctx_create(*this);
        }
        catch (const std::exception &e)
        {
            err = true;
            LOG_ERROR(e.what());
        }
    }

    if (err)
    {
        this->conn_ptr->is_close = true;
        this->worker_ptr->epoller.mod(conn_obj.fd, nullptr, event::event_poller::RWE, false);
        return;
    }
}

// socketobj connobj release
void websocket_ctx::on_close()
{
    this->conn_ptr = nullptr;
    this->worker_ptr = nullptr;
}

void websocket_ctx::on_event(uint32_t event)
{
    socket::socket *socket_ptr = &this->conn_ptr->socket_obj;
    connection *conn_ptr = this->conn_ptr;
    if (!socket_ptr->close_callback)
    {
        socket_ptr->close_callback = [socket_ptr]() {};
    }
    if (event & event::event_poller::ERR)
    {
        // LOG_ERROR("event::event_poller::ERR");
        conn_ptr->is_close = true;
    }
    if (conn_ptr->is_close)
    {
        if (conn_ptr->is_ready)
        {
            try
            {
                avant::app::websocket_app::on_close_connection(*this);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR(e.what());
            }
        }
        this->worker_ptr->close_client_fd(socket_ptr->get_fd());
        return;
    }

    // ssl not ready
    if (this->worker_ptr->use_ssl && !socket_ptr->get_ssl_accepted())
    {
        int ssl_status = SSL_accept(socket_ptr->get_ssl_instance());
        if (1 == ssl_status)
        {
            socket_ptr->set_ssl_accepted(true);
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

    // SSL is ready
    if (!this->is_connected) // to ws handshake
    {
        if (this->http_processed)
        {
            // LOG_ERROR("this->http_processed !this->is_connected");
            conn_ptr->is_close = true;
            this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
            return;
        }
        if (event & event::event_poller::READ) // recv http request header
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
                    break;
                }
                else if (len > 0)
                {
                    int nparsed = http_parser_execute(&this->http_parser_obj, websocket_ctx::settings.get(), buffer + buffer_len, len);
                    buffer_len += len;
                    if (this->http_parser_obj.upgrade)
                    {
                        this->is_upgrade = true;
                    }
                    else if (nparsed != len)
                    {
                        LOG_ERROR("nparsed != len");
                        len = 0;
                        conn_ptr->is_close = true;
                        this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                        return;
                    }
                }
                else
                {
                    // LOG_ERROR("socket->recv return len==0");
                    conn_ptr->is_close = true;
                    this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                    return;
                }
            } // while(true) parser http
        }

        if (!conn_ptr->is_close && !this->http_processed)
        {
            this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RE, false);
            return;
        }

        // is not websocket
        if (this->http_processed && !this->is_upgrade)
        {
            // LOG_ERROR("this->http_processed && !this->is_upgrade");
            conn_ptr->is_close = true;
            this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
            return;
        }

        // handshake
        if (!this->is_connected && this->is_upgrade && this->http_processed)
        {
            for (auto &header : this->headers)
            {
                if (header.first == "Sec-WebSocket-Key" && header.second.size() >= 1)
                {
                    this->sec_websocket_key = header.second[0];
                }
                if (header.first == "Sec-WebSocket-Version" && header.second.size() >= 1)
                {
                    this->sec_websocket_version = header.second[0];
                }
            }
            if (this->sec_websocket_key.empty() || this->sec_websocket_version != "13")
            {
                // LOG_ERROR("this->sec_websocket_key.empty() || this->sec_websocket_version != 13");
                conn_ptr->is_close = true;
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                return;
            }
            else
            {
                // response websocket connect header
                std::string combine_key = this->sec_websocket_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
                // calculate SHA1 hash
                avant::utility::SHA1 sha1;
                sha1.update(combine_key);
                std::string sha1_str = sha1.final();

                // Base64 encode the hash
                std::string base64_encoded = avant::utility::base64::base64_encode(sha1.to_binary(sha1_str));
                std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
                response += "Upgrade: websocket\r\n";
                response += "Connection: Upgrade\r\n";
                response += "Sec-WebSocket-Accept: " + base64_encoded + "\r\n\r\n";

                this->conn_ptr->send_buffer.append(response.c_str(), response.size());
                this->is_connected = true;
                this->conn_ptr->is_ready = true;
                this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);

                // notify app
                bool err = false;
                try
                {
                    avant::app::websocket_app::on_new_connection(*this);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR("websocket_app::on_new_connection err %s", e.what());
                    err = true;
                }
                if (err)
                {
                    conn_ptr->is_close = true;
                    this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                    return;
                }
            }
        }
    }

    if (!this->is_connected)
    {
        LOG_ERROR("!this->is_connected");
        this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
        return;
    }

    // connected status

    // read
    {
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
                    // LOG_ERROR("socket_ptr->recv len==0");
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
    }

    // process
    {
        bool err = false;
        try
        {
            avant::app::websocket_app::on_process_connection(*this);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("avant::app::websocket_app::on_process_connection %s", e.what());
            err = true;
        }
        if (err)
        {
            conn_ptr->is_close = true;
            this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
            return;
        }
    }

    // send
    {
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
                        // LOG_ERROR("socket_ptr->send len %d oper_errno != EAGAIN && oper_errno != EINTR && oper_errno != EWOULDBLOCK", len);
                        conn_ptr->is_close = true;
                    }
                    this->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                    break;
                }
            }
            break; // important
        }
    }
}

void websocket_ctx::add_header(const std::string &key, const std::string &value)
{
    auto res = this->headers.find(key);
    if (res == this->headers.end())
    {
        std::vector<std::string> m_vec;
        this->headers[key] = m_vec;
    }
    this->headers[key].push_back(value);
}

int websocket_ctx::send_data(const std::string &data)
{
    if (this->conn_ptr->is_close || this->conn_ptr->closed_flag)
    {
        // LOG_ERROR("this->conn_ptr->is_close || this->conn_ptr->closed_flag forbiden send_data %llu", this->conn_ptr->gid);
        return -1;
    }

    // need conn ready, forbiden to send_data
    if (!this->conn_ptr->is_ready)
    {
        // LOG_ERROR("!this->conn_ptr->is_ready %llu", this->conn_ptr->gid);
        return -2;
    }

    this->conn_ptr->send_buffer.append(data.c_str(), data.size());
    this->worker_ptr->epoller.mod(this->conn_ptr->fd, nullptr, event::event_poller::RWE, false);
    return 0;
}

uint64_t websocket_ctx::get_conn_gid()
{
    return this->conn_ptr->gid;
}

size_t websocket_ctx::get_recv_buffer_size()
{
    return this->conn_ptr->recv_buffer.size();
}

const char *websocket_ctx::get_recv_buffer_read_ptr()
{
    return this->conn_ptr->recv_buffer.get_read_ptr();
}

void websocket_ctx::recv_buffer_move_read_ptr_n(size_t n)
{
    return this->conn_ptr->recv_buffer.move_read_ptr_n(n);
}

size_t websocket_ctx::get_send_buffer_size()
{
    return this->conn_ptr->send_buffer.size();
}

void websocket_ctx::set_conn_is_close(bool val)
{
    this->conn_ptr->is_close = val;
}
