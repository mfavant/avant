#include "app/other_app.h"
#include "workers/other.h"
#include <avant-log/logger.h>
#include "utility/singleton.h"
#include "utility/time.h"
#include "app/lua_plugin.h"
#include "global/tunnel_id.h"
#include "proto/proto_util.h"

using avant::app::other_app;
namespace utility = avant::utility;
namespace global = avant::global;

void other_app::on_other_init(avant::workers::other &other_obj)
{
    LOG_ERROR("other_app::on_other_init()");
    utility::singleton<lua_plugin>::instance()->on_other_init(false);
}

void other_app::on_other_stop(avant::workers::other &other_obj)
{
    LOG_ERROR("other_app::on_other_stop()");
    utility::singleton<lua_plugin>::instance()->on_other_stop(false);
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
}

void other_app::on_other_tunnel(avant::workers::other &other_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package)
{
#if 0
    int sourcetunnelsid = tunnel_package.sourcetunnelsid();
    std::string targettunnelsid;

    for (int i = 0; i < tunnel_package.targettunnelsid().size(); i++)
    {
        targettunnelsid += std::to_string(tunnel_package.targettunnelsid().at(i)) + "_";
    }

    int targetallworker = tunnel_package.targetallworker();

    LOG_ERROR("other_app::on_other_tunnel() CMD %d sourcetunnelsid %d targettunnelsid %s targetallworker %d",
              package.cmd(),
              sourcetunnelsid,
              targettunnelsid.c_str(),
              targetallworker);
#endif
}
