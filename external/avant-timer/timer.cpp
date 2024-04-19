#include "timer.h"

using namespace avant::timer;

int64_t timer::s_initial_id = 0;
std::mutex timer::s_mutex;

timer::timer(int32_t repeated_times, int64_t interval, timer_callback callback)
{
    m_repeated_times = repeated_times;
    m_interval = interval;
    m_callback = callback;
    // 当前时间加上一个间隔时间,为下次到期时间
    m_expired_time = (int64_t)time(nullptr) + interval; // seconds
    m_id = generate_id();
}

timer::~timer()
{
}

int64_t timer::get_id() const
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

int64_t timer::generate_id()
{
    int64_t new_id;
    s_mutex.lock();
    ++s_initial_id;
    new_id = s_initial_id;
    s_mutex.unlock();
    return new_id;
}

time_t timer::get_expired_time() const
{
    return m_expired_time;
}
