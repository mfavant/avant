#pragma once
#include <cstdint>
#include <functional>
#include <utility>
#include <limits>

namespace avant
{
    namespace timer
    {
        class timer;

        // The callback must NOT modify the timer manager: do not add or delete
        // any timer from inside it (including re-adding this one).
        //
        // check_and_handle() walks the expiration queue while it invokes the
        // callback, holding a cached iterator across the call. Adding or
        // deleting a queued timer from within the callback could invalidate
        // that iterator (undefined behavior). Callbacks may read the manager
        // but must not write to it; schedule follow-up timers after the
        // callback returns, from the loop that drives check_and_handle().
        using timer_callback = std::function<void(const timer &)>;

        class timer final
        {
        public:
            constexpr static int32_t REPEAT_INFINITE = -1;

            timer(uint64_t timer_id,
                  uint64_t now_time_stamp,
                  int32_t repeated_times,
                  uint64_t interval,
                  timer_callback callback);

            ~timer() noexcept;

            [[nodiscard]] bool is_expired(uint64_t now) const noexcept;
            [[nodiscard]] uint64_t get_id() const noexcept;
            [[nodiscard]] int32_t get_repeated_times() const noexcept;
            [[nodiscard]] uint64_t get_expired_time() const noexcept;
            [[nodiscard]] uint64_t get_interval() const noexcept;

            // repeated_times == 0: a pending slot that will never fire (a one-shot timer
            // that has already fired also lands here once the manager drops it).
            [[nodiscard]] bool repeated_times_is_zero() const noexcept;

            void enforce_run() noexcept(false);

        private:
            void run() noexcept(false);
            friend class timer_manager;

        private:
            uint64_t m_id{0};                   // timer id
            uint64_t m_expired_time{0};         // expiry (same units as the clock)
            int32_t m_repeated_times{0};        // repeat count (-1 = infinite, 0 = arming)
            uint64_t m_interval{0};             // period between firings
            timer_callback m_callback{nullptr}; // callback
        };
    }
}
