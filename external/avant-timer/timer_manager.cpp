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

    auto iter = m_queue.begin();
    while (iter != m_queue.end())
    {
        auto next_iter = std::next(iter);

        {
            auto ptr = iter->second;
            bool keep = false;

            if (ptr->is_expired(now_time_stamp))
            {
                ptr->run();
                int32_t times = ptr->get_repeated_times();
                if (times != 0)
                {
                    keep = true;
                }
                m_queue.erase(iter); // 只要timer被run了就要删除
            }
            else
            {
                break; // 说明后面的也没过期不用再处理了
            }

            auto deleted_iter = m_marked_delete.find(iter->second->get_id());
            if (deleted_iter != m_marked_delete.end())
            {
                m_marked_delete.erase(deleted_iter);
                keep = false;
            }

            if (keep)
            {
                m_list.push_back(ptr);
            }
        }

        iter = next_iter;
    }

    for (auto iter = m_list.begin(); iter != m_list.end(); ++iter)
    {
        m_queue.insert({(*iter)->get_expired_time(), *iter});
    }
}

void timer_manager::mark_delete(uint64_t timer_id)
{
    this->m_marked_delete.insert(timer_id);
}

void timer_manager::clean_up()
{
    if (m_marked_delete.empty())
    {
        return;
    }

    auto iter = m_queue.begin();
    while (iter != m_queue.end())
    {
        auto next_iter = std::next(iter);

        {
            if (m_marked_delete.find(iter->second->get_id()) != m_marked_delete.end())
            {
                m_queue.erase(iter);
            }
        }

        iter = next_iter;
    }

    m_marked_delete.clear();
}