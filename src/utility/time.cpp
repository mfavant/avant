#include "utility/time.h"

using namespace avant::utility;

uint64_t time::get_milliseconds() const
{
    return std::chrono::time_point_cast<std::chrono::milliseconds>(m_time).time_since_epoch().count();
}

uint64_t time::get_seconds() const
{
    return std::chrono::time_point_cast<std::chrono::seconds>(m_time).time_since_epoch().count();
}

void time::update()
{
    m_time = std::chrono::system_clock::now();
}

uint64_t time::get_monotonic_milliseconds() const
{
    return std::chrono::time_point_cast<std::chrono::milliseconds>(m_monotonic_time).time_since_epoch().count();
}

uint64_t time::get_monotonic_seconds() const
{
    return std::chrono::time_point_cast<std::chrono::seconds>(m_monotonic_time).time_since_epoch().count();
}

void time::update_monotonic()
{
    m_monotonic_time = std::chrono::steady_clock::now();
}
