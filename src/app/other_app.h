#pragma once
#include <cstdint>
#include "workers/other.h"

namespace avant
{
    namespace server
    {
        class server;
    }
    namespace app
    {
        class other_app
        {
        public:
            static void on_other_init(avant::workers::other &other_obj);
            static void on_other_stop(avant::workers::other &other_obj);
            static void on_other_tick(avant::workers::other &other_obj);
            static void on_other_tunnel(avant::workers::other &other_obj, const ProtoPackage &package, const ProtoTunnelPackage &tunnel_package);
        };
    }
}