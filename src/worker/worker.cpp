#include "worker/worker.h"
#include <avant-log/logger.h>
#include <thread>
#include <chrono>

using namespace avant::worker;

worker::worker()
{
}

worker::~worker()
{
}

void worker::operator()()
{
    LOG_ERROR("worker::operator() start worker[%d]", this->worker_id);

    int num = -1;
    while (true)
    {
        num = this->epoller.wait(10);
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
        constexpr int buffer_size = 102400;
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
    ::close(fd);
    this->curr_connection_num->fetch_sub(1);
}

void worker::on_client_event(int fd, uint32_t event)
{
    LOG_ERROR("worker client fd evented");
    close_client_fd(fd);
    this->epoller.del(fd, nullptr, 0);
}

void worker::on_tunnel_process(ProtoPackage &message)
{
    int cmd = message.cmd();
    if (cmd == ProtoCmd::TUNNEL_MAIN2WORKER_NEW_CLIENT)
    {
        ProtoTunnelMain2WorkerNewClient package;
        if (package.ParseFromString(message.body()))
        {
            int new_client_fd = package.fd();
            close_client_fd(new_client_fd);
        }
        else
        {
            LOG_ERROR("package.ParseFromString(message.body()) failed");
        }
    }
    else
    {
        LOG_ERROR("worker::on_tunnel_process undefine cmd %d", cmd);
    }
}