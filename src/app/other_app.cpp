#include "app/other_app.h"
#include "workers/other.h"
#include <avant-log/logger.h>
#include "utility/singleton.h"
#include "app/lua_plugin.h"

using avant::app::other_app;
namespace utility = avant::utility;

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
}

void other_app::on_other_tunnel(avant::workers::other &other_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package)
{
    // int sourcetunnelsid = tunnel_package.sourcetunnelsid();
    // std::string targettunnelsid;

    // for (int i = 0; i < tunnel_package.targettunnelsid().size(); i++)
    // {
    //     targettunnelsid += std::to_string(tunnel_package.targettunnelsid().at(i)) + "_";
    // }

    // int targetallworker = tunnel_package.targetallworker();

    // LOG_ERROR("other_app::on_other_tunnel() CMD %d sourcetunnelsid %d targettunnelsid %s targetallworker %d",
    //           package.cmd(),
    //           sourcetunnelsid,
    //           targettunnelsid.c_str(),
    //           targetallworker);
}
