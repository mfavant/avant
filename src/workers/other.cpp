// other thread, support ipc
#include "workers/other.h"
#include "avant-log/logger.h"
#include "proto/proto_util.h"

using namespace avant::workers;

other::other()
{
}

other::~other()
{
}

void other::operator()()
{
    LOG_ERROR("other::operator() start");

    int num = -1;
    while (true)
    {
        num = this->epoller.wait(this->epoll_wait_time);
        if (this->to_stop)
        {
            break;
        }
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
}

void other::on_tunnel_event(uint32_t event)
{
    LOG_ERROR("other::on_tunnel_event");
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

            LOG_ERROR("other recv datasize %llu", sizeof(data_size) + data_size);

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
                    LOG_ERROR("other::on_tunnel_event tunnel_conn oper_errno %d", oper_errno);
                    this->to_stop = true;
                }
                break;
            }
        }
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

    int inner_cmd = tunnelPackage.innerprotopackage().cmd();
    LOG_ERROR("other::on_tunnel_process inner_cmd[%d]", inner_cmd);
}
