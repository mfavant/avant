#pragma once
#include <cstdint>
#include <chrono>

namespace avant::utility
{
    class time
    {
    public:
        uint64_t get_milliseconds() const;
        uint64_t get_seconds() const;
        void update();

        uint64_t get_monotonic_milliseconds() const;
        uint64_t get_monotonic_seconds() const;
        void update_monotonic();

    private:
        std::chrono::system_clock::time_point m_time{};
        std::chrono::steady_clock::time_point m_monotonic_time{};
    };
}
