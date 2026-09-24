#include "connection/ipc_stream_ctx.h"
#include <avant-log/logger.h>
#include "app/other_app.h"
#include "utility/comm_errno.h"

using namespace avant::connection;

ipc_stream_ctx::ipc_stream_ctx()
{
}

ipc_stream_ctx::~ipc_stream_ctx()
{
}

void ipc_stream_ctx::on_create(connection &conn_obj, workers::other &other_obj)
{
    this->clear_app_layer_notified();
    this->conn_ptr = &conn_obj;
    this->other_ptr = &other_obj;

    this->conn_ptr->set_is_ready(true);

    bool err = false;
    try
    {
        if (this->other_ptr->is_remote2this(this->conn_ptr->get_gid()))
        {
            this->mark_app_layer_notified();
            avant::app::other_app::on_new_connection_remote2this(*this);
        }
        else if (this->other_ptr->is_this2remote(this->conn_ptr->get_gid()))
        {
            this->mark_app_layer_notified();
            avant::app::other_app::on_new_connection_this2remote(*this);
        }
        else
        {
            LOG_ERROR("ipc_ctx gid is not remote2this and this2remote gid {}", this->conn_ptr->get_gid());
        }
    }
    catch (const std::exception &e)
    {
        err = true;
        LOG_ERROR("{}", e.what());
    }

    if (err)
    {
        this->conn_ptr->set_is_close(true);
        event_mod(event::event_poller::RWE, false);
        return;
    }
}

void ipc_stream_ctx::on_close() noexcept
{
    this->conn_ptr = nullptr;
    this->other_ptr = nullptr;
}

int ipc_stream_ctx::send_data(const std::string &data, bool flush /*= true*/)
{
    if (this->conn_ptr->get_is_close() || this->conn_ptr->get_closed_flag())
    {
        return -1;
    }

    if (!this->conn_ptr->get_is_ready())
    {
        return -2;
    }

    this->conn_ptr->get_send_buffer().append(data.c_str(), data.size());
    if (flush)
    {
        try_send_flush();
    }
    else
    {
        event_mod(event::event_poller::RWE, false);
    }
    return 0;
}

uint64_t ipc_stream_ctx::get_conn_gid() const
{
    return this->conn_ptr->get_gid();
}

size_t ipc_stream_ctx::get_recv_buffer_size() const
{
    return this->conn_ptr->get_recv_buffer().size();
}

const char *ipc_stream_ctx::get_recv_buffer_read_ptr() const
{
    return this->conn_ptr->get_recv_buffer().get_read_ptr();
}

void ipc_stream_ctx::recv_buffer_move_read_ptr_n(size_t n)
{
    this->conn_ptr->get_recv_buffer().move_read_ptr_n(n);
}

size_t ipc_stream_ctx::get_send_buffer_size() const
{
    return this->conn_ptr->get_send_buffer().size();
}

void ipc_stream_ctx::set_conn_is_close(bool val)
{
    this->conn_ptr->set_is_close(val);
}

void ipc_stream_ctx::try_send_flush()
{
    avant::socket::socket *socket_ptr = &this->conn_ptr->get_socket_obj();
    avant::connection::connection *conn_ptr = this->conn_ptr;
    avant::utility::vec_str_buffer &send_buffer = conn_ptr->get_send_buffer();
    if (send_buffer.empty())
    {
        event_mod(event::event_poller::RE, false);
        return;
    }
    while (!send_buffer.empty())
    {
        int oper_errno = 0;
        int len = socket_ptr->send(send_buffer.get_read_ptr(), send_buffer.size(), oper_errno);
        if (len > 0)
        {
            conn_ptr->record_sent_bytes(len);
            send_buffer.move_read_ptr_n(len);
        }
        else
        {
            if (oper_errno != avant::utility::comm_errno::COMM_ERRNO_EAGAIN &&
                oper_errno != avant::utility::comm_errno::COMM_ERRNO_EINTR &&
                oper_errno != avant::utility::comm_errno::COMM_ERRNO_EWOULDBLOCK)
            {
                LOG_ERROR("ipc_stream_ctx client sock send data oper_errno {}", oper_errno);
                conn_ptr->set_is_close(true);
            }
            event_mod(event::event_poller::RWE, false);
            break;
        }
    }
}

void ipc_stream_ctx::on_event(uint32_t event) noexcept
{
    if (this->conn_ptr == nullptr || this->other_ptr == nullptr)
    {
        return;
    }

    avant::socket::socket &socket_ref = this->conn_ptr->get_socket_obj();
    avant::socket::socket *socket_ptr = &socket_ref;
    avant::connection::connection *conn_ptr = this->conn_ptr;
    if (!socket_ptr->close_callback)
    {
        socket_ptr->close_callback = [socket_ptr]()
        {
            if (socket_ptr)
            {
                // LOG_DEBUG("ipc_stream_ctx socket close_callback fd {}", socket_ptr->get_fd());
            }
        };
    }

    if (event & event::event_poller::ERR)
    {
        conn_ptr->set_is_close(true);
    }

    if (conn_ptr->get_is_close())
    {
        if (conn_ptr->get_is_ready())
        {
            try
            {
                avant::app::other_app::on_close_connection(*this);
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("{}", e.what());
            }
        }

        this->other_ptr->close_ipc_client_fd(socket_ptr->get_fd());
        return;
    }

    // IPC is plaintext; no TLS on the other-thread channel.

    // read from socket,parse protocol and process
    if (event & event::event_poller::READ)
    {
        int oper_errno = 0;
        int len = -1;
        constexpr int buffer_size = 10240;
        std::vector<char> buffer(buffer_size);
        int buffer_len = 0;

        while (buffer_len < buffer_size)
        {
            len = socket_ptr->recv(buffer.data() + buffer_len, buffer_size - buffer_len, oper_errno);

            if (len == -1 &&
                (oper_errno == avant::utility::comm_errno::COMM_ERRNO_EAGAIN ||
                 oper_errno == avant::utility::comm_errno::COMM_ERRNO_EWOULDBLOCK))
            {
                len = 0;
                break;
            }
            else if (len == -1 &&
                     oper_errno == avant::utility::comm_errno::COMM_ERRNO_EINTR)
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
                conn_ptr->set_is_close(true);
                event_mod(event::event_poller::RWE, false);
                return;
            }
        }

        if (buffer_len > 0)
        {
            conn_ptr->record_recv_bytes(buffer_len);
            conn_ptr->get_recv_buffer().append(buffer.data(), buffer_len);
        }
    }

    if (conn_ptr->get_recv_buffer().size() > 0)
    {
        bool err = false;
        try
        {
            avant::app::other_app::on_process_connection(*this);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("avant::app::other_app::on_process_connection {}", e.what());
            err = true;
        }
        if (err)
        {
            conn_ptr->set_is_close(true);
            event_mod(event::event_poller::RWE, false);
            return;
        }
    }

    // write to socket
    while (event & event::event_poller::WRITE)
    {
        try_send_flush();
        break; // important
    }

    return;
}

std::pair<std::string, int> ipc_stream_ctx::get_ip_port() const
{
    if (this->conn_ptr == nullptr)
    {
        return {};
    }

    return this->conn_ptr->get_socket_obj().get_realtime_ip_port();
}
