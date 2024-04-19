#pragma once
#include <atomic>
#include "event/event_poller.h"

namespace avant::worker
{
    class worker
    {
    public:
        worker();
        ~worker();
        void operator()();

        bool to_stop{false};
        bool is_stoped{false};
        int worker_id{-1};
        size_t max_client_num{0};
        std::atomic<int> *curr_connection_num{nullptr};

        avant::event::event_poller epoller;
    };
};