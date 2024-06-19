#pragma once
#include "server/server.h"
#include "workers/worker.h"
#include "workers/other.h"

namespace avant::hooks
{
    class tick
    {
    public:
        // main event loop tick
        static void on_main_tick(avant::server::server &server_obj);
        // worker event loop tick
        static void on_worker_tick(avant::workers::worker &worker_obj);
        // other event loop tick
        static void on_other_tick(avant::workers::other &other_obj);
    };
}