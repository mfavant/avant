#include "timer_manager.h"
#include <ctime>
#include <iostream>

using namespace avant::timer;

bool timer_manager::TimerComparator::operator()(const timer *a, const timer *b) const
{
    return a->get_expired_time() > b->get_expired_time();
}

timer_manager::timer_manager()
{
}

timer_manager::~timer_manager()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty())
    {
        timer *timer_ptr = m_queue.top();
        delete timer_ptr;
        m_queue.pop();
    }
}

int64_t timer_manager::add(int32_t repeated_times, int64_t interval, const timer_callback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    timer *timer_ptr = new timer(repeated_times, interval, callback);
    if (timer_ptr == nullptr)
    {
        return -1;
    }
    m_queue.push(timer_ptr);
    return timer_ptr->get_id();
}

void timer_manager::check_and_handle()
{
    time_t now = ::time(nullptr);
    std::lock_guard<std::mutex> lock(m_mutex);
    std::list<timer *> m_list;
    while (!m_queue.empty())
    {
        auto ptr = m_queue.top();
        if (ptr->is_expired(now))
        {
            ptr->run();
            int32_t times = ptr->get_repeated_times();
            if (times == 0)
            {
                delete ptr;
            }
            else
            {
                m_list.push_back(ptr);
            }
            m_queue.pop();
        }
        else
        {
            break;
        }
    }
    for (auto iter = m_list.begin(); iter != m_list.end(); ++iter)
    {
        m_queue.push(*iter);
    }
}
