#pragma once
#include <functional>
#include <mutex>
#include <cstdint>
#include <ctime>

namespace avant
{
    namespace timer
    {
        using timer_callback = std::function<void()>;
        class timer
        {
        public:
            timer(int32_t repeated_times, int64_t interval, timer_callback callback);
            timer(const timer &) = delete;
            virtual ~timer();
            virtual void run();
            bool is_expired(time_t now) const;
            int64_t get_id() const;
            int32_t get_repeated_times() const;
            time_t get_expired_time() const;

        public:
            static int64_t generate_id();

        private:
            int64_t m_id{0};                    // timer id
            time_t m_expired_time{0};           // 到期时间
            int32_t m_repeated_times{0};        // 重复次数
            timer_callback m_callback{nullptr}; // 回调函数
            int64_t m_interval{0};              // 时间间隔

        private:
            static int64_t s_initial_id;
            static std::mutex s_mutex; // 保护 s_initial_id 线程安全
        };
    }
}