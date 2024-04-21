#pragma once

#include "server/server.h"
#include "worker/worker.h"

namespace avant::hooks
{
    class stop
    {
    public:
        // main loop stop after
        static void on_main_stop(avant::server::server &server_obj);
        // worker loop stop after
        static void on_worker_stop(avant::worker::worker &worker_obj);
    };
}