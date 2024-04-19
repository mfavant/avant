#include "worker/worker.h"
#include <avant-log/logger.h>
#include <thread>
#include <chrono>

using namespace avant::worker;

worker::worker()
{
}

worker::~worker()
{
}

void worker::operator()()
{
    LOG_ERROR("worker::operator() start worker[%d]", this->worker_id);

    int latest_show = 0;
    int num = -1;
    while (true)
    {
        num = this->epoller.wait(10);
        if (num < 0)
        {
            LOG_ERROR("epoller.wait return [%d]", num);
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                break;
            }
        }

        int inow = this->curr_connection_num->fetch_add(1);
        if (inow - latest_show > 100)
        {
            LOG_ERROR("curr_connection_num %d", inow);
            latest_show = inow;
        }
        if (this->to_stop)
        {
            break;
        }
    }

    LOG_ERROR("worker::operator() end worker[%d]", this->worker_id);
    this->is_stoped = true;
}