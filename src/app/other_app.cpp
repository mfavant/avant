#include "app/other_app.h"
#include "workers/other.h"
#include <avant-log/logger.h>
#include "utility/singleton.h"
#include "utility/time.h"
#include "app/lua_plugin.h"
#include "global/tunnel_id.h"
#include "server/server.h"
#include "proto/proto_util.h"
#include "proto_res/proto_example.pb.h"
#include "proto_res/proto_ipc_stream.pb.h"
#include <unordered_map>

using avant::app::other_app;
namespace utility = avant::utility;
namespace global = avant::global;

class avant_authenticated_ipc_pair
{
public:
    std::unordered_map<uint64_t, std::string> gid2appid;
    std::unordered_map<std::string, uint64_t> appid2gid;
};

static avant_authenticated_ipc_pair authenticated_ipc_pair;

void other_app::on_other_init(avant::workers::other &other_obj)
{
    LOG_ERROR("other_app::on_other_init()");
    utility::singleton<lua_plugin>::instance()->on_other_init();
}

void other_app::on_other_stop(avant::workers::other &other_obj)
{
    LOG_ERROR("other_app::on_other_stop()");
    utility::singleton<lua_plugin>::instance()->on_other_stop();
}

void other_app::on_other_tick(avant::workers::other &other_obj)
{
    // LOG_ERROR("other_app::on_other_tick()");
    utility::singleton<lua_plugin>::instance()->on_other_tick();

    static utility::time time_component;
    static time_t latest_tick_time = 0;
    time_component.update();
    time_t tick_time = time_component.get_seconds();
    if (std::abs(tick_time - latest_tick_time) >= 5)
    {
        // send to all worker
        {
            ProtoTunnelOther2WorkerTest other2worker_test;
            other2worker_test.set_time(tick_time);
            std::vector<int> vec_worker_all_tunnel_id{};
            global::tunnel_id::get().get_worker_all_tunnel_id(vec_worker_all_tunnel_id);
            ProtoPackage package;
            avant::proto::pack_package(package, other2worker_test, ProtoCmd::PROTO_CMD_TUNNEL_OTHER2WORKER_TEST);
            other_obj.tunnel_forward(vec_worker_all_tunnel_id, package);
        }
        latest_tick_time = tick_time;
    }

    if constexpr (false)
    {
        // test ipc message process
        for (const auto &item : authenticated_ipc_pair.gid2appid)
        {
            uint64_t gid = item.first;
            avant::connection::connection *conn = other_obj.ipc_connection_mgr.get()->get_conn_by_gid(gid);
            if (!conn)
            {
                continue;
            }
            avant::connection::ipc_stream_ctx *ctx = dynamic_cast<avant::connection::ipc_stream_ctx *>(conn->ctx_ptr.get());
            if (!ctx)
            {
                continue;
            }

            {
                ProtoPackage resPackage;
                ProtoCSReqExample req;
                req.set_testcontext("HELLO WORLD");
                std::string data;
                ctx->send_data(avant::proto::pack_package(data, avant::proto::pack_package(resPackage, req, ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE)));
            }
        }
    }
}

void other_app::on_other_tunnel(avant::workers::other &other_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package)
{
    if constexpr (false)
    {
        int sourcetunnelsid = tunnel_package.sourcetunnelsid();
        std::string targettunnelsid;

        for (int i = 0; i < tunnel_package.targettunnelsid().size(); i++)
        {
            targettunnelsid += std::to_string(tunnel_package.targettunnelsid().at(i)) + "_";
        }

        int targetallworker = tunnel_package.targetallworker();

        LOG_ERROR("other_app::on_other_tunnel() CMD {} sourcetunnelsid {} targettunnelsid {} targetallworker {}",
                  (int)package.cmd(),
                  sourcetunnelsid,
                  targettunnelsid.c_str(),
                  targetallworker);
    }
}

void other_app::on_new_connection_remote2this(avant::connection::ipc_stream_ctx &ctx)
{
    // ipc auth
    // this <-- connect -- remote
    // this -- whoami --> remote
    {
        ProtoPackage resPackage;
        ProtoIPCStreamAuthHandshake res;
        res.set_appid(ctx.get_other_ptr()->get_server()->get_config()->get_app_id());

        std::string data;
        ctx.send_data(avant::proto::pack_package(data, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE)));
    }
    // LOG_ERROR("other_app on_new_connection {}", ctx.get_conn_gid());
}

void other_app::on_close_connection(avant::connection::ipc_stream_ctx &ctx)
{
    uint64_t gid = ctx.get_conn_gid();

    // is ipc conn
    if (authenticated_ipc_pair.gid2appid.find(gid) != authenticated_ipc_pair.gid2appid.end())
    {
        // LOG_ERROR("close ipc_client gid {}", gid);
        std::string app_id = authenticated_ipc_pair.gid2appid[gid];
        authenticated_ipc_pair.gid2appid.erase(gid);
        authenticated_ipc_pair.appid2gid.erase(app_id);
    }
}

void other_app::on_process_connection(avant::connection::ipc_stream_ctx &ctx)
{
    if (ctx.get_recv_buffer_size() > 2048000)
    {
        ctx.set_conn_is_close(true);
        ctx.event_mod(nullptr, event::event_poller::RWE, false);
        LOG_ERROR("ctx.get_recv_buffer_size() > 2048000");
        return;
    }

    // parse protocol
    while (ctx.get_recv_buffer_size() > 0)
    {
        uint64_t data_size = 0;
        if (ctx.get_recv_buffer_size() >= sizeof(data_size))
        {
            data_size = *((uint64_t *)ctx.get_recv_buffer_read_ptr());
            data_size = avant::proto::toh64(data_size);

            if (data_size + sizeof(data_size) > ctx.get_recv_buffer_size())
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
            LOG_ERROR("other_app::on_process_connection data_size == 0");
            ctx.recv_buffer_move_read_ptr_n(sizeof(data_size));
            break;
        }

        ProtoPackage protoPackage;
        if (!protoPackage.ParseFromArray(ctx.get_recv_buffer_read_ptr() + sizeof(data_size), data_size))
        {
            LOG_ERROR("ipc_stream_ctx protoPackage.ParseFromArra failed {}", data_size);
            ctx.recv_buffer_move_read_ptr_n(sizeof(data_size) + data_size);
            break;
        }

        ctx.recv_buffer_move_read_ptr_n(sizeof(data_size) + data_size);

        // process protocol package
        on_recv_package(ctx, protoPackage);
    }
}

void other_app::on_recv_package(avant::connection::ipc_stream_ctx &ctx, const ProtoPackage &package)
{
    // LOG_ERROR("ipc_stream_ctx gid {} cmd {}", ctx.get_conn_gid(), package.cmd());
    if (package.cmd() == ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE)
    {
        ProtoCSReqExample req;
        if (avant::proto::parse(req, package))
        {
            ProtoPackage resPackage;
            ProtoCSResExample res;
            res.set_testcontext(req.testcontext());
            std::string data;
            ctx.send_data(avant::proto::pack_package(data, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE)));
        }
    }
    else if (package.cmd() == ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE)
    {
        ProtoCSResExample res;
        if (avant::proto::parse(res, package))
        {
            // res.testcontext();
        }
    }
    else if (package.cmd() == ProtoCmd::PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE)
    {
        // ipc auth
        // this <-- whoami -- remote
        if (ctx.get_other_ptr()->is_this2remote(ctx.get_conn_gid()))
        {
            ProtoPackage resPackage;
            ProtoIPCStreamAuthHandshake res;
            res.set_appid(ctx.get_other_ptr()->get_server()->get_config()->get_app_id());
            std::string data;
            ctx.send_data(avant::proto::pack_package(data, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE)));
        }

        ProtoIPCStreamAuthHandshake authInfo;
        if (avant::proto::parse(authInfo, package))
        {
            std::string auth_appId = authInfo.appid();
            uint64_t auth_gid = ctx.get_conn_gid();

            // {appId, gid} is a binary that has been successfully authenticated
            bool succ = false;
            if (authenticated_ipc_pair.appid2gid.find(auth_appId) == authenticated_ipc_pair.appid2gid.end())
            {
                if (authenticated_ipc_pair.gid2appid.find(auth_gid) == authenticated_ipc_pair.gid2appid.end())
                {
                    authenticated_ipc_pair.appid2gid[auth_appId] = auth_gid;
                    authenticated_ipc_pair.gid2appid[auth_gid] = auth_appId;
                    succ = true;
                }
            }

            if (succ)
            {
                LOG_ERROR("appId {}, auth_gid {} insert to authenticated_ipc_pair succ", auth_appId.c_str(), auth_gid);
            }
            else
            {
                LOG_ERROR("appId {}, auth_gid {} insert to authenticated_ipc_pair failed", auth_appId.c_str(), auth_gid);
            }
        }
    }
    else
    {
        LOG_ERROR("unknow cmd {}", (int)package.cmd());
    }
}

void other_app::on_udp_server_recvfrom(avant::workers::other &other_obj, const char *buffer,
                                       ssize_t len,
                                       const struct sockaddr_storage &addr,
                                       socklen_t addr_len)
{
    if (!other_obj.udp_svr_component.get())
    {
        LOG_ERROR("other udp_svr_component message_callback recv udp_svr_component is nullptr len {}", len);
        return;
    }
    ProtoPackage package;
    if (!package.ParseFromArray(buffer, len))
    {
        LOG_ERROR("other udp_svr_component message_callback recv ParseFromArray failed len {}", len);
        return;
    }

    if (package.cmd() != ProtoCmd::PROTO_CMD_CS_REQ_EXAMPLE)
    {
        LOG_ERROR("other udp_svr_component message_callback recv unknown cmd {} len {}", (int)package.cmd(), len);
        return;
    }

    ProtoCSReqExample req;
    if (avant::proto::parse(req, package))
    {
        ProtoPackage resPackage;
        ProtoCSResExample res;
        res.set_testcontext(req.testcontext());
        std::string data;
        avant::proto::pack_package(data, avant::proto::pack_package(resPackage, res, ProtoCmd::PROTO_CMD_CS_RES_EXAMPLE));
        int int_ret = other_obj.udp_svr_component.get()->udp_component_client("", 0, data.data(), data.size(), (struct sockaddr *)&addr, addr_len);
        if (int_ret != 0)
        {
            LOG_ERROR("other udp_svr_component message_callback recv len {} udp_component_client {}", len, int_ret);
        }
    }
}
