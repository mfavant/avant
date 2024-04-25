#include "app/stream_app.h"
#include "server/server.h"
#include "worker/worker.h"
#include <avant-log/logger.h>
#include "proto_res/proto_example.pb.h"
#include "proto/proto_util.h"
#include "global/tunnel_id.h"

using namespace avant::app;

void stream_app::on_main_init(avant::server::server &server_obj)
{
}

void stream_app::on_worker_init(avant::worker::worker &worker_obj)
{
}

void stream_app::on_main_stop(avant::server::server &server_obj)
{
}

void stream_app::on_worker_stop(avant::worker::worker &worker_obj)
{
}

void stream_app::on_main_tick(avant::server::server &server_obj)
{
}

void stream_app::on_worker_tick(avant::worker::worker &worker_obj)
{
}

bool stream_app::on_recved_packsize(avant::connection::stream_ctx &ctx, uint64_t size)
{
    return size <= 10240;
}

void stream_app::on_new_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_new_connection gid %llu", ctx.conn_ptr->gid);
}

void stream_app::on_close_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_close_connection gid %llu", ctx.conn_ptr->gid);
}

void stream_app::on_process_connection(avant::connection::stream_ctx &ctx)
{
    // LOG_ERROR("stream_app on_process_connection gid %llu", ctx.conn_ptr->gid);
    auto conn_ptr = ctx.conn_ptr;
    auto socket_ptr = &ctx.conn_ptr->socket_obj;
    // parse protocol
    while (!conn_ptr->recv_buffer.empty())
    {
        uint64_t data_size = 0;
        if (conn_ptr->recv_buffer.size() >= sizeof(data_size))
        {
            data_size = *((uint64_t *)conn_ptr->recv_buffer.get_read_ptr());
            data_size = avant::proto::toh64(data_size);

            if (!avant::app::stream_app::on_recved_packsize(ctx, data_size))
            {
                conn_ptr->is_close = true;
                conn_ptr->stream_ctx_ptr->worker_ptr->epoller.mod(socket_ptr->get_fd(), nullptr, event::event_poller::RWE, false);
                return;
            }

            if (data_size + sizeof(data_size) > conn_ptr->recv_buffer.size())
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
            conn_ptr->recv_buffer.move_read_ptr_n(sizeof(data_size));
            break;
        }

        ProtoPackage protoPackage;
        if (!protoPackage.ParseFromArray(conn_ptr->recv_buffer.get_read_ptr() + sizeof(data_size), data_size))
        {
            LOG_ERROR("stream ctx client protoPackage.ParseFromArra failed %llu", data_size);
            conn_ptr->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);
            break;
        }

        conn_ptr->recv_buffer.move_read_ptr_n(sizeof(data_size) + data_size);

        if (protoPackage.cmd() == ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE)
        {
            ProtoCSReqExample req;
            if (avant::proto::parse(req, protoPackage))
            {
                ProtoPackage resPackage;
                ProtoCSResExample res;
                res.set_testcontext(req.testcontext());

                // broase cast all connection in the process including this ctx self, async
                ctx.worker_ptr->send_client_forward_message(ctx.conn_ptr->gid, {ctx.conn_ptr->gid}, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));
                // send_sync_package(ctx, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));
            }
        }
    }
}

int stream_app::send_sync_package(avant::connection::stream_ctx &ctx, const ProtoPackage &package)
{
    std::string data;
    return ctx.send_data(avant::proto::pack_package(data, package));
}

void stream_app::on_worker_tunnel(avant::worker::worker &worker_obj, const ProtoPackage &package)
{
    int cmd = package.cmd();
    LOG_ERROR("not exist handler %d", cmd);
}

// APP DEMO
void stream_app::on_client_forward_message(avant::connection::stream_ctx &ctx, ProtoTunnelClientForwardMessage &message, bool self)
{
    int cmd = message.innerprotopackage().cmd();
    if (self)
    {
        if (cmd == ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE)
        {
            stream_app::send_sync_package(ctx, message.innerprotopackage());
        }
        else
        {
            LOG_ERROR("not exit handler %d", cmd);
        }
    }
    else
    {
        LOG_ERROR("not exit handler %d", cmd);
    }
}
