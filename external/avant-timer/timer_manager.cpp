#include "timer_manager.h"

using namespace avant::timer;

std::shared_ptr<timer> timer_manager::add(std::shared_ptr<timer> new_timer)
{
    if (new_timer == nullptr)
    {
        return nullptr;
    }
    if (new_timer->get_repeated_times() <= 0 && new_timer->get_repeated_times() != timer::REPEAT_INFINITE)
    {
        return nullptr;
    }
    if (new_timer->get_interval() == 0 && new_timer->get_repeated_times() != 1)
    {
        return nullptr;
    }

    const uint64_t timer_id = new_timer->get_id();

    // timer_id is unique.
    //
    // If an existing timer has the same ID, remove it first.
    auto index_it = m_timer_index.find(timer_id);

    if (index_it != m_timer_index.end())
    {
        m_queue.erase(index_it->second);
        m_timer_index.erase(index_it);
    }

    // Insert into the expiration queue.
    auto queue_it = m_queue.emplace(
        new_timer->get_expired_time(),
        new_timer);

    // Save the queue iterator for O(1) average lookup by timer ID.
    m_timer_index.emplace(
        timer_id,
        queue_it);

    return new_timer;
}

void timer_manager::check_and_handle(uint64_t now_time_stamp)
{
    for (auto queue_it = m_queue.begin();
         queue_it != m_queue.end();)
    {
        auto ptr = queue_it->second;

        // m_queue is sorted by expiration time.
        //
        // Once the first non-expired timer is encountered,
        // all following timers are also not expired.
        if (!ptr->is_expired(now_time_stamp))
        {
            break;
        }

        const uint64_t timer_id = ptr->get_id();

        // Remove the timer from the ID index first.
        m_timer_index.erase(timer_id);

        // Remove the current queue node.
        //
        // queue_it becomes the next element.
        queue_it = m_queue.erase(queue_it);

        // Run after removing the timer from the manager.
        //
        // This makes the manager state consistent while user code runs.
        ptr->run();

        // 防止 ptr->run() 中的回调函数把自己又 add 到 manager 了
        if (m_timer_index.find(timer_id) != m_timer_index.end())
        {
            // 暂时不处理，这样肯定是出大问题了
        }

        // run() may consume the last repetition, so this check must
        // happen AFTER run().
        if (ptr->get_repeated_times() != 0)
        {
            const uint64_t next_expired_time =
                ptr->get_expired_time();

            auto new_queue_it = m_queue.emplace(
                next_expired_time,
                ptr);

            m_timer_index.emplace(
                timer_id,
                new_queue_it);
        }
    }
}

void timer_manager::mark_delete(uint64_t timer_id)
{
    auto index_it = m_timer_index.find(timer_id);

    if (index_it == m_timer_index.end())
    {
        return;
    }

    // Erase the timer directly from the expiration queue.
    m_queue.erase(index_it->second);

    // Erase the ID -> queue iterator mapping.
    m_timer_index.erase(index_it);
}

std::size_t timer_manager::size() const noexcept
{
    return m_queue.size();
}

bool timer_manager::empty() const noexcept
{
    return m_queue.empty();
}

bool timer_manager::exist(uint64_t timer_id) const noexcept
{
    return m_timer_index.find(timer_id) != m_timer_index.end();
}
