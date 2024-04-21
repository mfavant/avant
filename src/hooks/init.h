#pragma once

#include "server/server.h"
#include "worker/worker.h"

namespace avant::hooks
{
    class init
    {
    public:
        // main event loop before
        static void on_main_init(avant::server::server &server_obj);
        // worker event loop before
        static void on_worker_init(avant::worker::worker &worker_obj);
    };
}