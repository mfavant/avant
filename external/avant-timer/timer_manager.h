#pragma once
#include <memory>
#include "timer.h"
#include <unordered_set>
#include <map>

namespace avant
{
    namespace timer
    {
        class timer_manager final
        {
        public:
            timer_manager();
            ~timer_manager();

            std::shared_ptr<timer> add(std::shared_ptr<timer> new_timer);

            /**
             * @brief 检测定时器，到期则触发执行
             *
             */
            void check_and_handle(uint64_t now_time_stamp);

            void mark_delete(uint64_t timer_id);
            void clean_up();

        private:
            std::multimap<uint64_t, std::shared_ptr<timer>> m_queue;
            std::unordered_set<uint64_t> m_marked_delete;
        };
    }
}