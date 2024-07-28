#pragma once
#include "server/server.h"
#include "workers/worker.h"
#include "workers/other.h"

namespace avant::hooks
{
    class reload
    {
    public:
        static void on_cmd_reload(avant::server::server &server_obj);
    };
}