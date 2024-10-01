// other thread, support ipc
#include "workers/other.h"
#include "avant-log/logger.h"
#include "proto/proto_util.h"
#include "hooks/init.h"
#include "hooks/stop.h"
#include "hooks/tick.h"
#include "app/other_app.h"
#include "global/tunnel_id.h"
#include "connection/ipc_stream_ctx.h"
#include "socket/client_socket.h"
#include <pthread.h>
#include <signal.h>

using namespace avant::workers;
using namespace avant::global;
using namespace avant::connection;
using namespace avant::socket;

other::other()
{
}

other::~other()
{
}

int other::init_call_by_server()
{
    // Find the self configuration of this process from IPC JSON
    bool found = false;
    for (auto &item : this->ipc_json)
    {
        if (item["app_id"] == this->app_id)
        {
            found = true;
            this->ipc_self_json = item;
            break;
        }
    }

    if (false == found)
    {
        LOG_ERROR("this process json app_id %s not be found", this->app_id.c_str());
        return -1;
    }

    std::string app_id = this->ipc_self_json["app_id"].as_string();
    std::string ip = this->ipc_self_json["ip"].as_string();
    int port = this->ipc_self_json["port"].as_integer();

    LOG_ERROR("ipc self json app_id[%s] ip[%s] port[%d]", app_id.c_str(), ip.c_str(), port);

    // Create IPC listen socket
    avant::socket::server_socket *ipc_listen_socket = new (std::nothrow) avant::socket::server_socket(ip, port, this->max_ipc_conn_num);
    if (!ipc_listen_socket)
    {
        LOG_ERROR("new ipc_listen_socket failed");
        return -2;
    }

    this->ipc_listen_socket.reset(ipc_listen_socket);

    if (0 > this->ipc_listen_socket->get_fd())
    {
        LOG_ERROR("ipc_listen_socket fd less 0");
        return -3;
    }

    if (0 != this->epoller.add(this->ipc_listen_socket->get_fd(), nullptr, event::event_poller::RE, false))
    {
        LOG_ERROR("ipc_listen_socket epoller add failed");
        return -4;
    }

    return 0;
}

void other::operator()()
{
    LOG_ERROR("other::operator() start");

    {
        sigset_t set;
        sigfillset(&set);
        if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
        {
            LOG_ERROR("other pthread_sigmask failed");
            exit(-1);
        }
    }

    this->m_gid_seq = 0;
    this->m_other_loop_time.update();
    this->m_latest_tick_time = this->m_other_loop_time.get_seconds();

    ipc_client_to_connect();

    hooks::init::on_other_init(*this);
    int num = -1;
    while (true)
    {
        num = this->epoller.wait(this->epoll_wait_time);
        if (num < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                LOG_ERROR("other epoller.wait return [%d] errno %d", num, errno);
                break;
            }
        }

        if (this->to_stop)
        {
            break;
        }

        {
            this->m_other_loop_time.update();
            uint64_t now_time = this->m_other_loop_time.get_seconds();
            if (this->m_latest_tick_time != now_time)
            {
                this->m_gid_seq = 0;
                this->m_latest_tick_time = now_time;
            }
        }

        hooks::tick::on_other_tick(*this); // maybe change errno
        for (int i = 0; i < num; i++)
        {
            int evented_fd = this->epoller.m_events[i].data.fd;
            // main other tunnel fd
            if (evented_fd == this->main_other_tunnel->get_other())
            {
                on_tunnel_event(this->epoller.m_events[i].events);
            }
            else if (evented_fd == this->ipc_listen_socket->get_fd())
            {
                on_ipc_listen_event(this->epoller.m_events[i].events);
            }
            else if (this->ipc_connection_mgr->get_conn(evented_fd))
            {
                on_ipc_client_event(evented_fd, this->epoller.m_events[i].events);
            }
            // default unknow fd
            else
            {
                LOG_ERROR("unknow fd event");
                this->to_stop = true;
                break;
            }
        }
    }

    LOG_ERROR("other::operator() end");
    this->to_stop = true;
    this->is_stoped = true;
    hooks::stop::on_other_stop(*this);
}

int other::tunnel_forward(const std::vector<int> &dest_tunnel_id, ProtoPackage &message, bool flush /*= true*/)
{
    avant::socket::socket_pair &tunnel = *this->main_other_tunnel;
    // find conn
    connection::connection *tunnel_conn = this->ipc_connection_mgr->get_conn(tunnel.get_other());
    if (!tunnel_conn)
    {
        LOG_ERROR("other::tunnel_forward tunnel_conn is null");
        return -1;
    }
    avant::socket::socket &sock = tunnel.get_other_socket();

    std::unordered_set<int> dest_tunnel_id_set;
    // filter
    for (const int &dest : dest_tunnel_id)
    {
        if (tunnel_id::get().get_main_tunnel_id() == dest)
        {
            LOG_ERROR("other::tunnel_forward tunnel_id::get().get_main_tunnel_id() == dest");
            continue;
        }
        if (!tunnel_id::get().is_tunnel_id(dest))
        {
            LOG_ERROR("other::tunnel_forward !tunnel_id::get().is_tunnel_id(dest)");
            continue;
        }
        dest_tunnel_id_set.insert(dest);
    }

    if (dest_tunnel_id_set.empty())
    {
        LOG_ERROR("other::tunnel_forward dest_tunnel_id_set.empty()");
        return -1;
    }

    ProtoTunnelPackage tunnelPackage;
    tunnelPackage.set_sourcetunnelsid(tunnel_id::get().get_other_tunnel_id()); // other
    for (const int &dest : dest_tunnel_id_set)
    {
        tunnelPackage.mutable_targettunnelsid()->Add(dest);
    }
    tunnelPackage.mutable_innerprotopackage()->CopyFrom(message);

    // send to main
    std::string data;
    proto::pack_package(data, tunnelPackage, ProtoCmd::PROTO_CMD_TUNNEL_PACKAGE);

    tunnel_conn->send_buffer.append(data.c_str(), data.size());
    if (flush)
    {
        try_send_flush_tunnel();
    }
    else
    {
        this->epoller.mod(sock.get_fd(), nullptr, event::event_poller::RWE, false);
    }
    return 0;
}

void other::try_send_flush_tunnel()
{
    avant::socket::socket_pair &tunnel = *this->main_other_tunnel;
    // find conn
    connection::connection *tunnel_conn = this->ipc_connection_mgr->get_conn(tunnel.get_other());
    if (!tunnel_conn)
    {
        LOG_ERROR("other::on_tunnel_event tunnel_conn is null");
        return;
    }
    avant::socket::socket &sock = tunnel.get_other_socket();

    if (tunnel_conn->send_buffer.empty())
    {
        this->epoller.mod(sock.get_fd(), nullptr, event::event_poller::RE, false);
        return;
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
                LOG_ERROR("other::on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                this->to_stop = true;
            }
            else
            {
                this->epoller.mod(sock.get_fd(), nullptr, event::event_poller::RWE, false);
            }
            break;
        }
    }
}

void other::on_tunnel_event(uint32_t event)
{
    // LOG_ERROR("other::on_tunnel_event");
    avant::socket::socket_pair &tunnel = *this->main_other_tunnel;
    // find conn
    connection::connection *tunnel_conn = this->ipc_connection_mgr->get_conn(tunnel.get_other());
    if (!tunnel_conn)
    {
        LOG_ERROR("other::on_tunnel_event tunnel_conn is null");
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
                    LOG_ERROR("other::on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
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
                LOG_ERROR("other parseFromArray err %llu", data_size);
                tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
                break;
            }

            // LOG_ERROR("other recv datasize %llu", sizeof(data_size) + data_size);

            on_tunnel_process(protoPackage);
            tunnel_conn->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
        }
    }

    // check if there is any content that needs to be sent
    while (event & event::event_poller::WRITE)
    {
        try_send_flush_tunnel();
        break; // important
    }
}

void other::on_tunnel_process(ProtoPackage &message)
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
        LOG_ERROR("other !tunnelPackage.ParseFromString(message.protocol())");
        return;
    }

    try
    {
        app::other_app::on_other_tunnel(*this, tunnelPackage.innerprotopackage(), tunnelPackage);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("app on_other_tunnel throw exception cmd %d", tunnelPackage.innerprotopackage().cmd());
    }
}

uint64_t other::other_gen_gid()
{
    return (this->m_latest_tick_time << 32) | this->m_gid_seq++;
}

void other::on_ipc_listen_event(uint32_t event)
{
    for (size_t i = 0; i < this->accept_per_tick; i++)
    {
        int new_ipc_client_fd = this->ipc_listen_socket->accept();
        if (new_ipc_client_fd < 0)
        {
            break;
        }
        on_new_ipc_client_fd(new_ipc_client_fd, other_gen_gid());
    }
}

void other::close_ipc_client_fd(int fd)
{
    auto conn_ptr = this->ipc_connection_mgr->get_conn(fd);
    if (conn_ptr)
    {
        uint64_t gid = conn_ptr->get_gid();
        this->epoller.del(fd, nullptr, 0);
        int iret = this->ipc_connection_mgr->release_connection(fd);
        if (iret != 0)
        {
            LOG_ERROR("ipc_connection_mgr->release_connection(%d) failed", fd);
        }

        auto this2remote_gid2appid_iter = this->m_this2remote_gid2appid.find(gid);
        if (this2remote_gid2appid_iter != this->m_this2remote_gid2appid.end())
        {
            auto appid2gid_iter = this->m_this2remote_appid2gid.find(this2remote_gid2appid_iter->second);
            if (appid2gid_iter != this->m_this2remote_appid2gid.end())
            {
                this->m_this2remote_appid2gid.erase(appid2gid_iter);
            }
            LOG_ERROR("appid %s and %s connect failed", this->app_id.c_str(), this2remote_gid2appid_iter->second.c_str());
            this->m_this2remote_gid2appid.erase(this2remote_gid2appid_iter);

#if 1
            // TODO: reconnect remote app_id in here
            ipc_client_to_connect();
#endif
        }

        auto remote2this_gid_iter = this->m_remote2this_gid.find(gid);
        if (remote2this_gid_iter != this->m_remote2this_gid.end())
        {
            this->m_remote2this_gid.erase(remote2this_gid_iter);
        }
    }
    else
    {
        LOG_ERROR("worker close_client_fd conn_ptr is null, ::close %d", fd);
        ::close(fd);
    }
}

void other::on_new_ipc_client_fd(int fd, uint64_t gid)
{
    if (fd < 0)
    {
        LOG_ERROR("other::on_new_ipc_client_fd fd %d < 0", fd);
        return;
    }

    int iret = this->ipc_connection_mgr->alloc_connection(fd, gid);
    if (iret != 0)
    {
        LOG_ERROR("ipc_connection_mgr alloc_connection(%d,%llu) failed, iret %d", fd, gid, iret);
        close_ipc_client_fd(fd);
        return;
    }

    avant::connection::connection *ipc_conn = this->ipc_connection_mgr->get_conn(fd);
    if (!ipc_conn)
    {
        LOG_ERROR("ipc_connection_mgr->get_conn(%d) failed", fd);
        close_ipc_client_fd(fd);
        return;
    }

    if (!ipc_conn->ctx_ptr)
    {
        connection::ipc_stream_ctx *new_ctx = new (std::nothrow) connection::ipc_stream_ctx;
        if (!new_ctx)
        {
            LOG_ERROR("new connection::ipc_stream_ctx failed");
            close_ipc_client_fd(fd);
            return;
        }
        ipc_conn->ctx_ptr.reset(new_ctx);
    }

    if (!dynamic_cast<connection::ipc_stream_ctx *>(ipc_conn->ctx_ptr.get()))
    {
        LOG_ERROR("!dynamic_cast<connection::ipc_stream_ctx *>(ipc_conn->ctx_ptr.get())");
        close_ipc_client_fd(fd);
        return;
    }

    {
        ipc_conn->socket_obj.set_fd(fd);
        ipc_conn->socket_obj.close_callback = nullptr;
        ipc_conn->socket_obj.set_non_blocking();
        ipc_conn->socket_obj.set_linger(false, 0);
        ipc_conn->socket_obj.set_send_buffer(65536);
        ipc_conn->socket_obj.set_recv_buffer(65536);

        iret = this->epoller.add(fd, nullptr, event::event_poller::RWE, false);
        if (iret != 0)
        {
            LOG_ERROR("this->epoller.add ipc client fd failed");
            close_ipc_client_fd(fd);
            return;
        }
    }

    this->m_remote2this_gid.insert(gid);

    dynamic_cast<connection::ipc_stream_ctx *>(ipc_conn->ctx_ptr.get())->on_create(*ipc_conn, *this);
}

void other::on_ipc_client_event(int fd, uint32_t event)
{
    auto conn = this->ipc_connection_mgr->get_conn(fd);
    if (!conn)
    {
        LOG_ERROR("ipc_connection_mgr->get_conn failed fd %d", fd);
        close_ipc_client_fd(fd);
        return;
    }

    conn->ctx_ptr->on_event(event);
}

void other::ipc_client_to_connect()
{
    for (auto item : this->ipc_json)
    {
        if (item["app_id"].as_string() == this->app_id)
        {
            continue;
        }
        std::string app_id = item["app_id"].as_string();
        std::string ip = item["ip"].as_string();
        int port = item["port"].as_integer();

        if (this->m_this2remote_appid2gid.find(app_id) != this->m_this2remote_appid2gid.end())
        {
            continue;
        }

        socket::client_socket new_client_socket(ip, port);
        if (new_client_socket.get_fd() <= 0)
        {
            LOG_ERROR("new_client_socket.get_fd() <= 0");
            continue;
        }
        int iret = this->ipc_connection_mgr->alloc_connection(new_client_socket.get_fd(), this->other_gen_gid());
        if (iret != 0)
        {
            LOG_ERROR("this->ipc_connection_mgr->alloc_connection failed iret %d", iret);
            continue;
        }

        auto conn = this->ipc_connection_mgr->get_conn(new_client_socket.get_fd());

        if (!conn)
        {
            LOG_ERROR("this->ipc_connection_mgr->get_conn failed %d", new_client_socket.get_fd());
            continue;
        }

        if (!conn->ctx_ptr)
        {
            connection::ipc_stream_ctx *new_ctx = new (std::nothrow) connection::ipc_stream_ctx;
            if (!new_ctx)
            {
                LOG_ERROR("new connection::ipc_stream_ctx failed");
                continue;
            }
            conn->ctx_ptr.reset(new_ctx);
        }

        if (!dynamic_cast<connection::ipc_stream_ctx *>(conn->ctx_ptr.get()))
        {
            LOG_ERROR("!dynamic_cast<connection::ipc_stream_ctx *>(conn->ctx_ptr.get())");
            continue;
        }

        iret = this->epoller.add(new_client_socket.get_fd(), nullptr, event::event_poller::RWE, false);
        if (iret != 0)
        {
            LOG_ERROR("this->epoller.add ipc client fd failed");
            return;
        }

        conn->socket_obj = std::move(new_client_socket);
        this->m_this2remote_appid2gid[app_id] = conn->get_gid();
        this->m_this2remote_gid2appid[conn->get_gid()] = app_id;

        dynamic_cast<connection::ipc_stream_ctx *>(conn->ctx_ptr.get())->on_create(*conn, *this);
    }
}

bool other::is_this2remote(uint64_t gid) const
{
    return (this->m_this2remote_gid2appid.find(gid) != this->m_this2remote_gid2appid.end());
}

bool other::is_remote2this(uint64_t gid) const
{
    return (this->m_remote2this_gid.find(gid) != this->m_remote2this_gid.end());
}