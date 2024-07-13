#include "workers/worker.h"
#include <avant-log/logger.h>
#include <thread>
#include <chrono>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#include <unordered_set>
#include "hooks/tick.h"
#include "hooks/init.h"
#include "hooks/stop.h"
#include "connection/http_ctx.h"
#include "connection/websocket_ctx.h"
#include "connection/stream_ctx.h"
#include "global/tunnel_id.h"
#include "proto/proto_util.h"
#include "app/http_app.h"
#include "app/stream_app.h"
#include "app/websocket_app.h"

using namespace avant::workers;
using namespace avant::global;
using namespace avant::proto;

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
        num = this->epoller.wait(this->epoll_wait_time);
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
    this->to_stop = true;
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
        LOG_ERROR("worker::on_tunnel_event tunnel_conn is null");
        return;
    }
    avant::socket::socket &sock = tunnel.get_other_socket();

    // check if there is any content that needs to be read
    if (event & event::event_poller::READ)
    {
        constexpr int buffer_size = 1024000;
        char buffer[buffer_size]{0};
        int buffer_len = 0;

        while (buffer_len < buffer_size)
        {
            int len = 0;
            int oper_errno = 0;
            len = sock.recv(buffer + buffer_len, buffer_size - buffer_len, oper_errno);
            if (len > 0)
            {
                buffer_len += len;
            }
            else
            {
                if (oper_errno != EAGAIN && oper_errno != EINTR && oper_errno != EWOULDBLOCK)
                {
                    LOG_ERROR("worker::on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    this->to_stop = true;
                }
                break;
            }
        }
        if (buffer_len > 0)
        {
            tunnel_conn->recv_buffer.append(buffer, buffer_len);
        }

        // parser protocol
        while (!tunnel_conn->recv_buffer.empty())
        {
            uint64_t data_size = 0;
            if (tunnel_conn->recv_buffer.size() >= sizeof(data_size))
            {
                data_size = *((uint64_t *)tunnel_conn->recv_buffer.get_read_ptr());
                data_size = avant::proto::toh64(data_size);
                if (data_size + sizeof(data_size) > tunnel_conn->recv_buffer.size())
                {
                    break;
                }
            }
            else
            {
                break;
            }

            if (data_size == 0)
            {
                tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size));
                break;
            }

            ProtoPackage protoPackage;
            if (!protoPackage.ParseFromArray(tunnel_conn->recv_buffer.get_read_ptr() + sizeof(data_size), data_size))
            {
                LOG_ERROR("worker parseFromArray err %llu", data_size);
                tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
                break;
            }

            // LOG_ERROR("worker recv datasize %llu", sizeof(data_size) + data_size);

            on_tunnel_process(protoPackage);
            tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
        }
    }

    // check if there is any content that needs to be sent
    while (event & event::event_poller::WRITE)
    {
        if (tunnel_conn->send_buffer.empty())
        {
            this->epoller.mod(sock.get_fd(), nullptr, event::event_poller::RE, false);
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
                if (oper_errno != EAGAIN && oper_errno != EINTR && oper_errno != EWOULDBLOCK)
                {
                    LOG_ERROR("worker::on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    this->to_stop = true;
                }
                break;
            }
        }
        break; // important
    }
}

void worker::close_client_fd(int fd)
{
    if (this->worker_connection_mgr->get_conn(fd))
    {
        this->epoller.del(fd, nullptr, 0);
        int iret = this->worker_connection_mgr->release_connection(fd);
        if (iret != 0)
        {
            LOG_ERROR("worker_connection_mgr->release_connection(%d) failed", fd);
        }
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

int worker::tunnel_forward(const std::vector<int> &dest_tunnel_id, ProtoPackage &message)
{
    std::unordered_set<int> dest_tunnel_id_set;
    // filter
    for (const int &dest : dest_tunnel_id)
    {
        if (tunnel_id::get().get_main_tunnel_id() == dest)
        {
            LOG_ERROR("worker::tunnel_forward tunnel_id::get().get_main_tunnel_id() == dest");
            continue;
        }
        if (!tunnel_id::get().is_tunnel_id(dest))
        {
            LOG_ERROR("worker::tunnel_forward !tunnel_id::get().is_tunnel_id(dest)");
            continue;
        }
        dest_tunnel_id_set.insert(dest);
    }

    if (dest_tunnel_id_set.empty())
    {
        LOG_ERROR("worker::tunnel_forward dest_tunnel_id_set.empty()");
        return -1;
    }

    ProtoTunnelPackage tunnelPackage;
    tunnelPackage.set_sourcetunnelsid(tunnel_id::get().get_worker_tunnel_id(this->worker_id)); // this worker
    for (const int &dest : dest_tunnel_id_set)
    {
        tunnelPackage.mutable_targettunnelsid()->Add(dest);
    }
    tunnelPackage.mutable_innerprotopackage()->CopyFrom(message);

    // send to main
    std::string data;
    proto::pack_package(data, tunnelPackage, ProtoCmd::PROTO_CMD_TUNNEL_PACKAGE);

    connection::connection *tunnel_conn = this->worker_connection_mgr->get_conn(main_worker_tunnel->get_other());
    if (!tunnel_conn)
    {
        LOG_ERROR("tunnel_conn get failed");
        return -2;
    }

    tunnel_conn->send_buffer.append(data.c_str(), data.size());
    this->epoller.mod(this->main_worker_tunnel->get_other(), nullptr, event::event_poller::RWE, false);
    return 0;
}

void worker::handle_tunnel_client_forward_message(avant::connection::connection *conn_ptr, ProtoTunnelClientForwardMessage &message, const ProtoTunnelPackage &tunnel_package)
{
    if (conn_ptr->closed_flag || conn_ptr->is_close)
    {
        return;
    }

    try
    {
        switch (this->type)
        {
        case task::task_type::HTTP_TASK:
        {
            if (conn_ptr->is_ready && conn_ptr->http_ctx_ptr)
            {
                LOG_ERROR("task type err");
            }
            break;
        }
        case task::task_type::STREAM_TASK:
        {
            if (conn_ptr->is_ready && conn_ptr->stream_ctx_ptr) // is stream task,here is_ready important
            {
                avant::app::stream_app::on_client_forward_message(*conn_ptr->stream_ctx_ptr, conn_ptr->gid == message.sourcegid(), message, tunnel_package);
            }
            break;
        }
        case task::task_type::WEBSOCKET_TASK:
        {
            if (conn_ptr->is_ready && conn_ptr->websocket_ctx_ptr)
            {
                avant::app::websocket_app::on_client_forward_message(*conn_ptr->websocket_ctx_ptr, conn_ptr->gid == message.sourcegid(), message, tunnel_package);
            }
            break;
        }
        default:
        {
            LOG_ERROR("task type err");
            break;
        }
        }
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("process PROTO_CMD_TUNNEL_CLIENT_FORWARD_MESSAGE err");
    }
}

void worker::on_tunnel_process(ProtoPackage &message)
{
    int cmd = message.cmd();

    if (cmd != ProtoCmd::PROTO_CMD_TUNNEL_PACKAGE)
    {
        LOG_ERROR("cmd %d != PROTO_CMD_TUNNEL_PACKAGE", cmd);
        return;
    }

    ProtoTunnelPackage tunnelPackage;
    if (!tunnelPackage.ParseFromString(message.protocol()))
    {
        LOG_ERROR("worker !tunnelPackage.ParseFromString(message.protocol())");
        return;
    }

    int inner_cmd = tunnelPackage.innerprotopackage().cmd();

    if (inner_cmd == ProtoCmd::PROTO_CMD_TUNNEL_MAIN2WORKER_NEW_CLIENT)
    {
        ProtoTunnelMain2WorkerNewClient package;
        if (!avant::proto::parse(package, tunnelPackage.innerprotopackage()))
        {
            LOG_ERROR("ProtoTunnelMain2WorkerNewClient.ParseFromString failed cmd %d", inner_cmd);
            return;
        }
        on_new_client_fd(package.fd(), package.gid());
    }
    else if (inner_cmd == ProtoCmd::PROTO_CMD_TUNNEL_CLIENT_FORWARD_MESSAGE)
    {
        ProtoTunnelClientForwardMessage message;
        if (!avant::proto::parse(message, tunnelPackage.innerprotopackage()))
        {
            LOG_ERROR("parse ProtoTunnelClientForwardMessage err");
            return;
        }
        int this_worker_tunnelid = avant::global::tunnel_id::get().get_worker_tunnel_id(this->worker_id);
        // checking target Gid
        if (message.targetgid().empty()) // broadcase all client conn
        {
            uint64_t all_conn_in_this_worker = this->worker_connection_mgr->size();
            for (size_t i = 0; i < all_conn_in_this_worker; i++)
            {
                // maybe here conn_ptr is tunnel conn、ssl not ready、already marked close
                auto conn_ptr = this->worker_connection_mgr->get_conn_by_idx(i);
                if (!conn_ptr)
                {
                    break;
                }
                this->handle_tunnel_client_forward_message(conn_ptr, message, tunnelPackage);
            }
        }
        else
        {
            for (int i = 0; i < message.targetgid().size(); i++)
            {
                uint64_t target_gid = message.targetgid().Get(i);
                if (this_worker_tunnelid != avant::global::tunnel_id::get().hash_gid_2_worker_tunnel_id(target_gid))
                {
                    LOG_ERROR("it's not the worker client gid");
                    continue;
                }
                auto conn_ptr = this->worker_connection_mgr->get_conn_by_gid(target_gid);
                if (!conn_ptr)
                {
                    LOG_ERROR("process ProtoTunnelClientForwardMessage can not find targetgid %llu conn_ptr", target_gid);
                    continue;
                }
                this->handle_tunnel_client_forward_message(conn_ptr, message, tunnelPackage);
            }
        }
    }
    else // user's protocol
    {
        try
        {
            switch (this->type)
            {
            case task::task_type::HTTP_TASK:
                avant::app::http_app::on_worker_tunnel(*this, tunnelPackage.innerprotopackage(), tunnelPackage);
                break;
            case task::task_type::STREAM_TASK:
                avant::app::stream_app::on_worker_tunnel(*this, tunnelPackage.innerprotopackage(), tunnelPackage);
                break;
            case task::task_type::WEBSOCKET_TASK:
                avant::app::websocket_app::on_worker_tunnel(*this, tunnelPackage.innerprotopackage(), tunnelPackage);
                break;
            default:
                LOG_ERROR("task type err");
                break;
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("app on_worker_tunnel throw exception cmd %d", tunnelPackage.innerprotopackage().cmd());
        }
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

    // connection alloc success for new client fd

    // create task context for connection
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
                close_client_fd(fd);
                return;
            }
            conn->websocket_ctx_ptr = new_ctx;
        }
        create_conn_succ = true;
    }
    else if (this->type == task::task_type::NONE)
    {
        LOG_ERROR("this->type == task::task_type::NONE");
        close_client_fd(fd);
        return;
    }
    else
    {
        LOG_ERROR("this->type == undefine");
        close_client_fd(fd);
        return;
    }

    if (!create_conn_succ)
    {
        LOG_ERROR("create_conn_succ failed");
        close_client_fd(fd);
        return;
    }

    // create context for connection success, to reset it
    {
        // reset socket
        conn->socket_obj.set_fd(fd);
        conn->socket_obj.close_callback = nullptr;
        conn->socket_obj.set_non_blocking();
        conn->socket_obj.set_linger(false, 0);
        conn->socket_obj.set_send_buffer(65536);
        conn->socket_obj.set_recv_buffer(65536);

        iret = this->epoller.add(fd, nullptr, event::event_poller::RWE, false);
        if (iret != 0)
        {
            LOG_ERROR("this->epoller.add failed");
            close_client_fd(fd);
            return;
        }

        // SSL
        if (use_ssl)
        {
            if (conn->socket_obj.get_ssl_instance())
            {
                LOG_ERROR("SSL conn->socket_obj.get_ssl_instance() already not null");
                close_client_fd(fd);
                return;
            }
            bool ssl_err = false;
            SSL *ssl_instance = SSL_new(this->ssl_context); // socket close auto free SSL Object
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
            else // ssl failed
            {
                LOG_ERROR("SSL ERR");
                close_client_fd(fd);
                return;
            }
        }

        // triger context be created for connection
        if (conn->http_ctx_ptr)
        {
            conn->http_ctx_ptr->on_create(*conn, *this, false);
        }
        else if (conn->stream_ctx_ptr)
        {
            conn->stream_ctx_ptr->on_create(*conn, *this);
        }
        else if (conn->websocket_ctx_ptr)
        {
            conn->websocket_ctx_ptr->on_create(*conn, *this);
        }
    }
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
    auto conn = this->worker_connection_mgr->get_conn(fd);
    if (!conn)
    {
        LOG_ERROR("worker_connection_mgr->get_conn failed");
        close_client_fd(fd);
        return;
    }
    conn->stream_ctx_ptr->on_event(event);
}

void worker::on_client_event_websocket(int fd, uint32_t event)
{
    auto conn = this->worker_connection_mgr->get_conn(fd);
    if (!conn)
    {
        LOG_ERROR("worker_connection_mgr->get_conn failed");
        close_client_fd(fd);
        return;
    }
    conn->websocket_ctx_ptr->on_event(event);
}

int worker::send_client_forward_message(uint64_t source_gid, const std::set<uint64_t> &dest_conn_gid, ProtoPackage &package)
{
    if (avant::global::tunnel_id::get().hash_gid_2_worker_tunnel_id(source_gid) != avant::global::tunnel_id::get().get_worker_tunnel_id(this->worker_id))
    {
        LOG_ERROR("source id err");
        return -1;
    }

    std::unordered_map<uint64_t, std::vector<uint64_t>> target_tunnel_id;
    for (const uint64_t &conn_gid : dest_conn_gid)
    {
        target_tunnel_id[avant::global::tunnel_id::get().hash_gid_2_worker_tunnel_id(conn_gid)].push_back(conn_gid);
    }

    ProtoTunnelClientForwardMessage forwardMessage;
    forwardMessage.set_sourcegid(source_gid);
    forwardMessage.mutable_innerprotopackage()->CopyFrom(package);

    if (dest_conn_gid.empty())
    {
        // send to all worker
        std::vector<int> all_target_tunnel;
        for (int i = avant::global::tunnel_id::get().get_worker_tunnel_id_min(); i <= avant::global::tunnel_id::get().get_worker_tunnel_id_max(); i++)
        {
            all_target_tunnel.push_back(i);
        }
        ProtoPackage willBeForward;
        tunnel_forward(all_target_tunnel, avant::proto::pack_package(willBeForward, forwardMessage, ProtoCmd::PROTO_CMD_TUNNEL_CLIENT_FORWARD_MESSAGE));
    }
    else
    {
        ProtoPackage willBeForward;
        std::vector<int> all_target_tunnel;
        for (const auto &tunnel_id : target_tunnel_id)
        {
            forwardMessage.mutable_targetgid()->Clear();
            for (const auto &conn_gid : tunnel_id.second)
            {
                forwardMessage.mutable_targetgid()->Add(conn_gid);
            }
            all_target_tunnel.clear();
            all_target_tunnel.push_back(tunnel_id.first);
            tunnel_forward(all_target_tunnel, avant::proto::pack_package(willBeForward, forwardMessage, ProtoCmd::PROTO_CMD_TUNNEL_CLIENT_FORWARD_MESSAGE));
        }
    }

    return 0;
}