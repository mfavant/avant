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

    private:
        std::chrono::system_clock::time_point m_time{};
    };
}
