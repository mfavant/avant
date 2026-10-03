#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>

#include "timer.h"

namespace avant
{
    namespace timer
    {
        class timer_manager;

        // Single-threaded timer manager.
        //
        // All operations must be called from the same owning thread.
        // timer_id must be unique within this manager.
        class timer_manager final
        {
        private:
            using queue_type = std::multimap<uint64_t, std::shared_ptr<timer>>;

            using queue_iterator = queue_type::iterator;

        public:
            timer_manager() = default;
            ~timer_manager() = default;

            timer_manager(const timer_manager &) = delete;
            timer_manager &operator=(const timer_manager &) = delete;

            timer_manager(timer_manager &&) = delete;
            timer_manager &operator=(timer_manager &&) = delete;

            // Add a timer.
            //
            // timer_id must be unique. If a timer with the same ID already
            // exists, it is removed first and replaced.
            //
            // On success returns the stored new_timer. Returns nullptr when:
            //   - new_timer is nullptr, or
            //   - repeated_times is 0 (the "arming" state; a timer that will
            //     never fire), or
            //   - interval is 0 for a timer that fires more than once
            //     (repeated_times != 1); a zero interval would re-arm at the
            //     same expiry and fire on every tick.
            [[nodiscard]]
            std::shared_ptr<timer> add(std::shared_ptr<timer> new_timer);

            // Fire all timers whose expiration time <= now_time_stamp.
            //
            // A repeated timer is reinserted using its next expiration time.
            // A one-shot timer is removed after firing.
            void check_and_handle(uint64_t now_time_stamp);

            // Immediately remove the timer with the specified ID.
            //
            // Does nothing if the timer does not exist.
            void mark_delete(uint64_t timer_id);

            [[nodiscard]]
            std::size_t size() const noexcept;

            [[nodiscard]]
            bool empty() const noexcept;

            // Returns true if the timer exists, otherwise false.
            [[nodiscard]]
            bool exist(uint64_t timer_id) const noexcept;

        private:
            // Ordered by expiration time.
            queue_type m_queue;

            // timer_id -> corresponding node in m_queue.
            //
            // Allows O(1) average lookup and O(1) node removal.
            std::unordered_map<uint64_t, queue_iterator> m_timer_index;
        };
    }
}
