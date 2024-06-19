#pragma once

#include "server/server.h"
#include "workers/worker.h"
#include "workers/other.h"

namespace avant::hooks
{
    class stop
    {
    public:
        // main loop stop after
        static void on_main_stop(avant::server::server &server_obj);
        // worker loop stop after
        static void on_worker_stop(avant::workers::worker &worker_obj);
        // other loop stop after
        static void on_other_stop(avant::workers::other &other_obj);
    };
}