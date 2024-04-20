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

    int num = -1;
    while (true)
    {
        num = this->epoller.wait(10);
        if (this->to_stop)
        {
            break;
        }
        if (num < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                LOG_ERROR("worker epoller.wait return [%d] errno %d", num, errno);
                break;
            }
        }
        for (int i = 0; i < num; i++)
        {
            int evented_fd = this->epoller.m_events[i].data.fd;
            // main worker tunnel fd
            if (evented_fd == this->main_worker_tunnel->get_other())
            {
                LOG_ERROR("worker tunnel fd evented");
            }
            // default client fd
            else
            {
                LOG_ERROR("worker client fd evented");
                ::close(evented_fd);
                this->epoller.del(evented_fd, nullptr, 0);
            }
        }
    }

    LOG_ERROR("worker::operator() end worker[%d]", this->worker_id);
    this->is_stoped = true;
}