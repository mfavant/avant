#pragma once
#include <functional>
#include <cstdint>
#include <memory>

namespace avant
{
    namespace timer
    {
        class timer;
        using timer_callback = std::function<void(timer &)>;

        class timer
        {
        public:
            timer(uint64_t timer_id,
                  uint64_t now_time_stamp,
                  int32_t repeated_times,
                  uint64_t interval,
                  timer_callback callback);

            timer(const timer &) = delete;
            virtual ~timer();
            virtual void run();
            bool is_expired(time_t now) const;
            uint64_t get_id() const;
            int32_t get_repeated_times() const;
            time_t get_expired_time() const;

        private:
            uint64_t m_id{0};                   // timer id
            time_t m_expired_time{0};           // 到期时间
            int32_t m_repeated_times{0};        // 重复次数
            timer_callback m_callback{nullptr}; // 回调函数
            uint64_t m_interval{0};             // 时间间隔
        };

        class timer_comparator
        {
        public:
            bool operator()(std::shared_ptr<timer> a, std::shared_ptr<timer> b) const;
        };
    }
}
