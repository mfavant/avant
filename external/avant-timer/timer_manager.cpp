#include "timer_manager.h"
#include <ctime>
#include <iostream>
#include <list>

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
    m_queue.insert({new_timer->get_expired_time(), new_timer});
    return new_timer;
}

void timer_manager::check_and_handle(uint64_t now_time_stamp)
{
    std::list<std::shared_ptr<timer>> m_list;

    for (auto iter = m_queue.begin(); iter != m_queue.end();)
    {
        auto ptr = iter->second;

        auto deleted_iter = m_marked_delete.find(iter->second->get_id());
        if (deleted_iter != m_marked_delete.end()) // 已标记被删除
        {
            m_marked_delete.erase(deleted_iter);
            iter = m_queue.erase(iter);
            continue;
        }

        if (ptr->is_expired(now_time_stamp)) // 到期了
        {
            ptr->run();
            int32_t times = ptr->get_repeated_times();
            if (times != 0)
            {
                m_list.push_back(ptr);
            }
            iter = m_queue.erase(iter); // 只要timer被run了就要删除
            continue;
        }
        else
        {
            break; // m_queue升序遍历后面的时间都比iter大了，没必要继续遍历了
        }
    }

    for (auto iter = m_list.begin(); iter != m_list.end(); ++iter)
    {
        m_queue.insert({(*iter)->get_expired_time(), *iter});
    }

    m_marked_delete.clear();
}

void timer_manager::mark_delete(uint64_t timer_id)
{
    this->m_marked_delete.insert(timer_id);
}
