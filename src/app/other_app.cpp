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
#include "proto_res/proto_tunnel.pb.h"
#include <unordered_map>

using avant::app::other_app;
namespace utility = avant::utility;
namespace global = avant::global;

class avant_authenticated_ipc_pair
{
public:
    avant_authenticated_ipc_pair()
    {
        this->other_obj = nullptr;
    }
    std::unordered_map<uint64_t, std::string> gid2appid;
    std::unordered_map<std::string, uint64_t> appid2gid;
    avant::workers::other *other_obj;
};

static avant_authenticated_ipc_pair authenticated_ipc_pair;

void other_app::on_other_init(avant::workers::other &other_obj)
{
    LOG_ERROR("other_app::on_other_init()");
    if (!authenticated_ipc_pair.other_obj)
    {
        authenticated_ipc_pair.other_obj = &other_obj;
    }

    utility::singleton<lua_plugin>::instance()->on_other_init(&other_obj);
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
    if (std::abs(tick_time - latest_tick_time) >= 10)
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
        for (auto item : authenticated_ipc_pair.gid2appid)
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
    // 从 worker 传到 other 线程 lua 虚拟机
    if (package.cmd() == ProtoCmd::PROOT_CMD_TUNNEL_WORKER2OTHER_LUAVM)
    {
        ProtoTunnelWorker2OtherLuaVM worker2OtherVMPackage;
        bool ret = avant::proto::parse(worker2OtherVMPackage, package);
        if (!ret)
        {
            LOG_ERROR("parse worker2OtherVMPackage from package failed");
            return;
        }
        uint64_t fromGid = worker2OtherVMPackage.gid();
        int worker_idx = worker2OtherVMPackage.workeridx();

        // LOG_ERROR("client conngid {} send cmd[{}]", fromGid, worker2OtherVMPackage.innerprotopackage().cmd());
        // 交给LuaVM处理
        avant::ProtoCmd cmd = worker2OtherVMPackage.innerprotopackage().cmd();

        // 必须写解包操作
        std::shared_ptr<google::protobuf::Message> ptrMessage = utility::singleton<lua_plugin>::instance()->protobuf_cmd2message(cmd);
        if (!ptrMessage)
        {
            LOG_ERROR("other_app::on_other_tunnel unknow cmd {}", (int)cmd);
            return;
        }

        ret = avant::proto::parse(*ptrMessage, worker2OtherVMPackage.innerprotopackage());
        if (!ret)
        {
            LOG_ERROR("other_app::on_other_tunnel unknow cmd {}", (int)cmd);
            return;
        }

        utility::singleton<lua_plugin>::instance()->on_other_lua_vm_recv_client_message(cmd,
                                                                                        *ptrMessage,
                                                                                        fromGid,
                                                                                        worker_idx);
    }
    else
    {
        LOG_ERROR("other_app::on_other_tunnel unknow cmd {}", (int)package.cmd());
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
        res.set_appid(ctx.get_other_ptr()->get_server()->get_config().get_app_id());

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
            data_size = avant::proto::toh64_from_buffer(ctx.get_recv_buffer_read_ptr());

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
    if (package.cmd() == ProtoCmd::PROTO_CMD_IPC_STREAM_AUTH_HANDSHAKE)
    {
        // ipc auth
        // this <-- whoami -- remote
        if (ctx.get_other_ptr()->is_this2remote(ctx.get_conn_gid()))
        {
            ProtoPackage resPackage;
            ProtoIPCStreamAuthHandshake res;
            res.set_appid(ctx.get_other_ptr()->get_server()->get_config().get_app_id());
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
        if (authenticated_ipc_pair.gid2appid.find(ctx.get_conn_gid()) == authenticated_ipc_pair.gid2appid.end())
        {
            LOG_ERROR("authenticated_ipc_pair.gid2appid.find(ctx.get_conn_gid()) == authenticated_ipc_pair.gid2appid.end()");
            return;
        }

        const std::string &from_app_id_string = authenticated_ipc_pair.gid2appid.find(ctx.get_conn_gid())->second;
        int worker_idx = avant::global::tunnel_id::get().get_other_tunnel_id();

        // 必须写解包操作
        std::shared_ptr<google::protobuf::Message> ptrMessage = utility::singleton<lua_plugin>::instance()->protobuf_cmd2message(package.cmd());
        if (!ptrMessage)
        {
            LOG_ERROR("other_app::on_recv_package unknow cmd {}", (int)package.cmd());
            return;
        }

        bool ret = avant::proto::parse(*ptrMessage, package);
        if (!ret)
        {
            LOG_ERROR("other_app::on_recv_package unknow cmd {}", (int)package.cmd());
            return;
        }

        utility::singleton<lua_plugin>::instance()->on_other_lua_vm_recv_ipc_message(package.cmd(),
                                                                                     *ptrMessage,
                                                                                     from_app_id_string);
    }
}

void other_app::other_lua_send_ipc_package(const std::string &app_id, int cmd, google::protobuf::Message &message)
{
    if (authenticated_ipc_pair.appid2gid.find(app_id) == authenticated_ipc_pair.appid2gid.end())
    {
        LOG_ERROR("other_app::other_lua_send_ipc_package appid2gid not found app_id[{}] cmd[{}]", app_id.c_str(), cmd);
        return;
    }
    uint64_t ipc_ctx_gid = authenticated_ipc_pair.appid2gid.find(app_id)->second;
    avant::connection::connection *ipc_conn = authenticated_ipc_pair.other_obj->ipc_connection_mgr->get_conn_by_gid(ipc_ctx_gid);
    if (!ipc_conn)
    {
        LOG_ERROR("other_app::other_lua_send_ipc_package !ipc_conn app_id[{}] cmd[{}]", app_id.c_str(), cmd);
        return;
    }

    avant::connection::ipc_stream_ctx *ipc_stream_ctx = dynamic_cast<avant::connection::ipc_stream_ctx *>(ipc_conn->ctx_ptr.get());
    if (!ipc_stream_ctx)
    {
        LOG_ERROR("other_app::other_lua_send_ipc_package !ipc_stream_ctx app_id[{}] cmd[{}]", app_id.c_str(), cmd);
        return;
    }

    ProtoPackage resPackage;
    resPackage.set_cmd((avant::ProtoCmd)cmd);
    std::string data;
    int ret = ipc_stream_ctx->send_data(avant::proto::pack_package(data, avant::proto::pack_package(resPackage, message, (avant::ProtoCmd)cmd)));
    if (ret != 0)
    {
        LOG_ERROR("other_app::other_lua_send_ipc_package ret[{}]!= 0 app_id[{}] cmd[{}]", ret, app_id.c_str(), cmd);
        return;
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

    int cmd = package.cmd();

    std::shared_ptr<google::protobuf::Message> ptrMessage = utility::singleton<lua_plugin>::instance()->protobuf_cmd2message(cmd);
    if (!ptrMessage)
    {
        LOG_ERROR("other_app::on_other_tunnel unknow cmd {}", cmd);
        return;
    }

    bool bool_ret = avant::proto::parse(*ptrMessage, package);
    if (!bool_ret)
    {
        LOG_ERROR("other_app::on_other_tunnel unknow cmd {}", cmd);
        return;
    }

    std::string from_ip = other_obj.udp_svr_component->udp_component_get_ip(addr);
    int from_port = other_obj.udp_svr_component->udp_component_get_port(addr);

    utility::singleton<lua_plugin>::instance()->on_other_lua_vm_recv_udp_message(cmd, *ptrMessage, from_ip, from_port);
}
