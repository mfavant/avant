#include "timer.h"

#include <cstdint>
#include <limits>

using namespace avant::timer;

timer::timer(uint64_t timer_id,
             uint64_t now_time_stamp,
             int32_t repeated_times,
             uint64_t interval,
             timer_callback callback)
    : m_id(timer_id),
      m_repeated_times(repeated_times),
      m_interval(interval),
      m_callback(std::move(callback))
{
    if (interval > UINT64_MAX - now_time_stamp)
    {
        // Saturate to UINT64_MAX to prevent the expiry time from wrapping around.
        m_expired_time = UINT64_MAX;
    }
    else
    {
        m_expired_time = now_time_stamp + interval; // seconds
    }
}

timer::~timer() noexcept = default;

uint64_t timer::get_id() const noexcept
{
    return m_id;
}

int32_t timer::get_repeated_times() const noexcept
{
    return m_repeated_times;
}

void timer::enforce_run() noexcept(false)
{
    run();
}

void timer::run()
{
    if (m_repeated_times == REPEAT_INFINITE || m_repeated_times >= 1)
    {
        if (m_callback)
        {
            m_callback(*this);
        }
    }
    if (m_repeated_times >= 1)
    {
        --m_repeated_times;
    }
    // Re-arm only when the timer will fire again; a one-shot timer keeps its
    // just-fired expiry so it cannot appear to be scheduled in the future.
    if (m_repeated_times != 0)
    {
        if (m_interval > UINT64_MAX - m_expired_time)
        {
            m_expired_time = UINT64_MAX;
        }
        else
        {
            m_expired_time += m_interval;
        }
    }
}

bool timer::is_expired(uint64_t now) const noexcept
{
    return now >= m_expired_time;
}

uint64_t timer::get_expired_time() const noexcept
{
    return m_expired_time;
}

uint64_t timer::get_interval() const noexcept
{
    return m_interval;
}

bool timer::repeated_times_is_zero() const noexcept
{
    return m_repeated_times == 0;
}
