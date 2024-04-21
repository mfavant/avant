#include "worker/worker.h"
#include <avant-log/logger.h>
#include <thread>
#include <chrono>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include "hooks/tick.h"
#include "hooks/init.h"
#include "hooks/stop.h"
#include "connection/http_ctx.h"
#include "connection/websocket_ctx.h"
#include "connection/stream_ctx.h"

using namespace avant::worker;

std::string worker::http_static_dir;

worker::worker()
{
}

worker::~worker()
{
}

void worker::operator()()
{
    LOG_ERROR("worker::operator() start worker[%d]", this->worker_id);
    hooks::init::on_worker_init(*this);
    int num = -1;
    while (true)
    {
        num = this->epoller.wait(10);
        if (this->to_stop)
        {
            break;
        }
        hooks::tick::on_worker_tick(*this);
        if (num < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                LOG_ERROR("worker epoller.wait return [%d] errno %d", num, errno);
                break;
            }
        }
        for (int i = 0; i < num; i++)
        {
            int evented_fd = this->epoller.m_events[i].data.fd;
            // main worker tunnel fd
            if (evented_fd == this->main_worker_tunnel->get_other())
            {
                on_tunnel_event(this->epoller.m_events[i].events);
            }
            // default client fd
            else
            {
                on_client_event(evented_fd, this->epoller.m_events[i].events);
            }
        }
    }
    LOG_ERROR("worker::operator() end worker[%d]", this->worker_id);
    this->is_stoped = true;
    hooks::stop::on_worker_stop(*this);
}

void worker::on_tunnel_event(uint32_t event)
{
    avant::socket::socket_pair &tunnel = *this->main_worker_tunnel;
    // find conn
    connection::connection *tunnel_conn = this->worker_connection_mgr->get_conn(tunnel.get_other());
    if (!tunnel_conn)
    {
        LOG_ERROR("on_tunnel_event tunnel_conn is null");
        return;
    }
    avant::socket::socket &sock = tunnel.get_other_socket();

    // check if there is any content that needs to be read
    if (event & EPOLLIN)
    {
        constexpr int buffer_size = 1024000;
        char buffer[buffer_size]{0};
        int buffer_used_idx{0};

        while (buffer_used_idx < buffer_size)
        {
            int len = 0;
            int oper_errno = 0;
            len = sock.recv(buffer + buffer_used_idx, buffer_size - buffer_used_idx, oper_errno);
            if (len > 0)
            {
                buffer_used_idx += len;
            }
            else
            {
                if (oper_errno != EAGAIN && oper_errno != EINTR)
                {
                    LOG_ERROR("on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    this->to_stop = true;
                }
                break;
            }
        }
        if (buffer_used_idx > 0)
        {
            tunnel_conn->recv_buffer.append(buffer, buffer_used_idx);
        }

        // parser protocol
        while (!tunnel_conn->recv_buffer.empty())
        {
            ProtoPackage protoPackage;
            uint64_t data_size = 0;
            if (tunnel_conn->recv_buffer.size() >= sizeof(data_size))
            {
                data_size = *((uint64_t *)tunnel_conn->recv_buffer.get_read_ptr());
                data_size = ::be64toh(data_size);
                if (data_size + sizeof(data_size) > tunnel_conn->recv_buffer.size())
                {
                    break;
                }
            }
            if (!protoPackage.ParseFromArray(tunnel_conn->recv_buffer.get_read_ptr() + sizeof(data_size), data_size))
            {
                break;
            }
            on_tunnel_process(protoPackage);
            tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
        }
    }

    // check if there is any content that needs to be sent
    while (event & EPOLLOUT)
    {
        if (tunnel_conn->send_buffer.empty())
        {
            this->epoller.mod(sock.get_fd(), nullptr, EPOLLIN | EPOLLERR);
            break;
        }

        while (!tunnel_conn->send_buffer.empty())
        {
            int oper_errno = 0;
            int len = sock.send(tunnel_conn->send_buffer.get_read_ptr(), tunnel_conn->send_buffer.size(), oper_errno);
            if (len > 0)
            {
                tunnel_conn->send_buffer.move_read_ptr_n(len);
            }
            else
            {
                if (oper_errno != EAGAIN && oper_errno != EINTR)
                {
                    LOG_ERROR("on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    this->to_stop = true;
                }
                break;
            }
        }
        break;
    }
}

void worker::close_client_fd(int fd)
{
    if (this->worker_connection_mgr->get_conn(fd))
    {
        this->epoller.del(fd, nullptr, 0);
        this->worker_connection_mgr->release_connection(fd);
    }
    else
    {
        ::close(fd);
    }
    this->curr_connection_num->fetch_sub(1);
}

void worker::on_client_event(int fd, uint32_t event)
{
    if (this->type == task::task_type::HTTP_TASK)
    {
        on_client_event_http(fd, event);
    }
    else if (this->type == task::task_type::STREAM_TASK)
    {
        on_client_event_stream(fd, event);
    }
    else if (this->type == task::task_type::WEBSOCKET_TASK)
    {
        on_client_event_websocket(fd, event);
    }
    else
    {
        LOG_ERROR("worker undefine type");
        close_client_fd(fd);
    }
}

void worker::on_tunnel_process(ProtoPackage &message)
{
    int cmd = message.cmd();
    if (cmd == ProtoCmd::TUNNEL_MAIN2WORKER_NEW_CLIENT)
    {
        ProtoTunnelMain2WorkerNewClient package;
        if (package.ParseFromString(message.body()))
        {
            on_new_client_fd(package.fd(), package.gid());
        }
        else
        {
            LOG_ERROR("package.ParseFromString(message.body()) failed cmd %d", cmd);
        }
    }
    else
    {
        LOG_ERROR("worker::on_tunnel_process undefine cmd %d", cmd);
    }
}

void worker::on_new_client_fd(int fd, uint64_t gid)
{
    // create new conn
    int iret = this->worker_connection_mgr->alloc_connection(fd, gid);
    if (iret != 0)
    {
        LOG_ERROR("worker_connection_mgr alloc_connection(%d,%llu) failed iret %d", fd, gid, iret);
        close_client_fd(fd);
        return;
    }
    // get new conn
    avant::connection::connection *conn = this->worker_connection_mgr->get_conn(fd);
    if (!conn)
    {
        LOG_ERROR("worker::on_new_client_fd worker_connection_mgr->get_conn failed");
        close_client_fd(fd);
        return;
    }
    bool create_conn_succ = false;
    if (this->type == task::task_type::HTTP_TASK)
    {
        if (!conn->http_ctx_ptr)
        {
            std::shared_ptr<connection::http_ctx> new_ctx;
            new_ctx.reset(new (std::nothrow) connection::http_ctx);
            if (!new_ctx.get())
            {
                LOG_ERROR("new connection::http_ctx failed");
                close_client_fd(fd);
                return;
            }
            conn->http_ctx_ptr = new_ctx;
        }
        create_conn_succ = true;
    }
    else if (this->type == task::task_type::STREAM_TASK)
    {
        if (!conn->stream_ctx_ptr)
        {
            std::shared_ptr<connection::stream_ctx> new_ctx;
            new_ctx.reset(new (std::nothrow) connection::stream_ctx);
            if (!new_ctx)
            {
                LOG_ERROR("new connection::stream_ctx failed");
                close_client_fd(fd);
                return;
            }
            conn->stream_ctx_ptr = new_ctx;
        }
        create_conn_succ = true;
    }
    else if (this->type == task::task_type::WEBSOCKET_TASK)
    {
        if (!conn->websocket_ctx_ptr)
        {
            std::shared_ptr<connection::websocket_ctx> new_ctx;
            new_ctx.reset(new (std::nothrow) connection::websocket_ctx);
            if (!new_ctx)
            {
                LOG_ERROR("new connection::websocket_ctx failed");
            }
            conn->websocket_ctx_ptr = new_ctx;
        }
        create_conn_succ = true;
    }
    else if (this->type == task::task_type::NONE)
    {
        LOG_ERROR("this->type == task::task_type::NONE");
        close_client_fd(fd);
    }
    else
    {
        LOG_ERROR("this->type == undefine");
        close_client_fd(fd);
    }
    if (create_conn_succ)
    {
        // reset socket
        conn->socket_obj.set_fd(fd);
        conn->socket_obj.close_callback = nullptr;
        conn->socket_obj.set_non_blocking();
        conn->socket_obj.set_linger(false, 0);
        conn->socket_obj.set_send_buffer(65536);
        conn->socket_obj.set_recv_buffer(65536);

        iret = this->epoller.add(fd, nullptr, EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLRDHUP);
        if (iret != 0)
        {
            LOG_ERROR("this->epoller.add failed");
            close_client_fd(fd);
            return;
        }

        // SSL
        if (use_ssl)
        {
            bool ssl_err = false;
            SSL *ssl_instance = SSL_new(this->ssl_context);
            if (!ssl_instance)
            {
                ssl_err = true;
                LOG_ERROR("SSL_new return NULL");
            }
            if (!ssl_err && 1 != SSL_set_fd(ssl_instance, fd))
            {
                ssl_err = true;
                LOG_ERROR("SSL_set_fd error %s", ERR_error_string(ERR_get_error(), nullptr));
            }
            if (!ssl_err && ssl_instance)
            {
                conn->socket_obj.set_ssl_instance(ssl_instance);
                conn->socket_obj.set_ssl_accepted(false);
            }
            else
            {
                LOG_ERROR("SSL ERR");
                close_client_fd(fd);
            }
        }

        if (conn->http_ctx_ptr)
        {
            conn->http_ctx_ptr->on_create(*conn, *this);
        }
        if (conn->stream_ctx_ptr)
        {
            conn->stream_ctx_ptr->on_create(*conn, *this);
        }
        if (conn->websocket_ctx_ptr)
        {
            conn->websocket_ctx_ptr->on_create(*conn, *this);
        }
    }
    else
    {
        LOG_ERROR("create_conn_succ failed");
    }
}

void worker::send_pack_to_tunnel(ProtoPackage &message)
{
    std::string data;
    message.SerializeToString(&data);
    {
        uint64_t data_size = data.size();
        data_size = ::htobe64(data_size);
        char *data_size_arr = (char *)&data_size;
        data.insert(0, data_size_arr, sizeof(data_size));
    }
    connection::connection *tunnel_conn = this->worker_connection_mgr->get_conn(main_worker_tunnel->get_other());
    if (!tunnel_conn)
    {
        LOG_ERROR("tunnel_conn get failed");
        return;
    }
    tunnel_conn->send_buffer.append(data.c_str(), data.size());
    this->epoller.mod(this->main_worker_tunnel->get_other(), nullptr, EPOLLIN | EPOLLOUT | EPOLLERR);
}

void worker::on_client_event_http(int fd, uint32_t event)
{
    auto conn = this->worker_connection_mgr->get_conn(fd);
    if (!conn)
    {
        LOG_ERROR("worker_connection_mgr->get_conn failed");
        close_client_fd(fd);
        return;
    }
    conn->http_ctx_ptr->on_event(event);
}

void worker::on_client_event_stream(int fd, uint32_t event)
{
}

void worker::on_client_event_websocket(int fd, uint32_t event)
{
}
