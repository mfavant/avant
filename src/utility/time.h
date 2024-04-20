#include <cstdint>
#include <chrono>

namespace avant::utility
{
    class time
    {
    public:
        uint64_t get_milliseconds();
        uint64_t get_seconds();
        void update();
        std::chrono::system_clock::time_point m_time;
    };
}