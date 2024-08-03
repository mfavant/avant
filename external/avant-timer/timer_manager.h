#pragma once
#include <queue>
#include <vector>
#include <list>
#include <memory>
#include "timer.h"

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

        private:
            std::priority_queue<std::shared_ptr<timer>, std::vector<std::shared_ptr<timer>>, timer_comparator> m_queue;
        };
    }
}