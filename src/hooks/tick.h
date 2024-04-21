#pragma once
#include "server/server.h"
#include "worker/worker.h"

namespace avant::hooks
{
    class tick
    {
    public:
        // main event loop tick
        static void on_main_tick(avant::server::server &server_obj);
        // worker event loop tick
        static void on_worker_tick(avant::worker::worker &worker_obj);
    };
}