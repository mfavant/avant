#pragma once

#include "server/server.h"
#include "workers/worker.h"
#include "workers/other.h"

namespace avant::hooks
{
    class init
    {
    public:
        // main event loop before
        static void on_main_init(avant::server::server &server_obj);
        // worker event loop before
        static void on_worker_init(avant::workers::worker &worker_obj);
        // other event loop before
        static void on_other_init(avant::workers::other &other_obj);
    };
}