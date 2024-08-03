#include "timer_manager.h"
#include <ctime>
#include <iostream>

using namespace avant::timer;

timer_manager::timer_manager()
{
}

timer_manager::~timer_manager()
{
}

std::shared_ptr<timer> timer_manager::add(std::shared_ptr<timer> new_timer)
{
    if (new_timer == nullptr)
    {
        return nullptr;
    }
    m_queue.push(new_timer);
    return new_timer;
}

void timer_manager::check_and_handle(uint64_t now_time_stamp)
{
    std::list<std::shared_ptr<timer>> m_list;
    while (!m_queue.empty())
    {
        auto ptr = m_queue.top();
        if (ptr->is_expired(now_time_stamp))
        {
            ptr->run();
            int32_t times = ptr->get_repeated_times();
            if (times != 0)
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
