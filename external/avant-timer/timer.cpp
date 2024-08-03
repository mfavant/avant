#include "timer.h"

using namespace avant::timer;

timer::timer(uint64_t timer_id,
             uint64_t now_time_stamp,
             int32_t repeated_times,
             uint64_t interval,
             timer_callback callback)
{
    m_id = timer_id;
    m_repeated_times = repeated_times;
    m_interval = interval;
    m_callback = callback;

    m_expired_time = now_time_stamp + interval; // seconds
}

timer::~timer()
{
}

uint64_t timer::get_id() const
{
    return m_id;
}

int32_t timer::get_repeated_times() const
{
    return m_repeated_times;
}

void timer::run()
{
    if (m_repeated_times == -1 || m_repeated_times >= 1)
    {
        m_callback();
    }
    if (m_repeated_times >= 1)
    {
        --m_repeated_times;
    }
    m_expired_time += m_interval;
}

bool timer::is_expired(time_t now) const
{
    return now >= m_expired_time;
}

time_t timer::get_expired_time() const
{
    return m_expired_time;
}

bool timer_comparator::operator()(std::shared_ptr<timer> a, std::shared_ptr<timer> b) const
{
    return a->get_expired_time() > b->get_expired_time();
}
