// other thread, support ipc
#include "workers/other.h"
#include "avant-log/logger.h"
#include "proto/proto_util.h"
#include "hooks/init.h"
#include "hooks/stop.h"
#include "hooks/tick.h"
#include "app/other_app.h"
#include "global/tunnel_id.h"
#include "pthread.h"

using namespace avant::workers;
using namespace avant::global;

other::other()
{
}

other::~other()
{
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

    hooks::init::on_other_init(*this);
    int num = -1;
    while (true)
    {
        num = this->epoller.wait(this->epoll_wait_time);
        if (this->to_stop)
        {
            break;
        }
        hooks::tick::on_other_tick(*this);
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
        for (int i = 0; i < num; i++)
        {
            int evented_fd = this->epoller.m_events[i].data.fd;
            // main other tunnel fd
            if (evented_fd == this->main_other_tunnel->get_other())
            {
                on_tunnel_event(this->epoller.m_events[i].events);
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
