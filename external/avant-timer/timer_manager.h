#pragma once
#include <queue>
#include <mutex>
#include <vector>
#include <list>
#include "timer.h"

namespace avant
{
    namespace timer
    {
        class timer_manager final
        {
            class TimerComparator
            {
            public:
                bool operator()(const timer *a, const timer *b) const;
            };

        public:
            timer_manager();
            ~timer_manager();
            /**
             * @brief 添加新的定时器
             *
             * @param repeated_times 重复次数，为-1则一直重复下去
             * @param interval 触发间隔
             * @param callback 回调函数
             * @return int64_t 返回新创建的定时器id
             */
            int64_t add(int32_t repeated_times, int64_t interval, const timer_callback callback);

            /**
             * @brief 检测定时器，到期则触发执行
             *
             */
            void check_and_handle();

        private:
            std::priority_queue<timer *, std::vector<timer *>, TimerComparator> m_queue;
            std::mutex m_mutex;
        };
    }
}