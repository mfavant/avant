#pragma once
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <atomic>
#include <chrono>
#include <mutex>
#include <format>
#include <iostream>

namespace avant
{
    namespace log
    {
        using namespace std;
        class logger
        {
        public:
            static logger &instance()
            {
                static logger m_instance;
                return m_instance;
            }

            logger();
            ~logger();

            enum flag
            {
                DEBUG,
                INFO,
                WARN,
                ERROR,
                FATAL,
                FLAG_COUNT
            };

            /**
             * @brief open log file directory
             *
             * @param log_file_base_path directory path for log files (e.g., "logs")
             * @param log_level one of DEBUG..FATAL
             * @return 0 on success, -1 if the directory/file could not be created or opened
             */
            int open(const std::string &log_file_base_path, const int log_level);

            template <typename... Args>
            void debug(const char *file, int line, const char *func, std::format_string<Args...> fmt, Args &&...args)
            {
                if (m_log_level > DEBUG)
                {
                    return;
                }
                log(DEBUG, file, line, func, fmt, std::forward<Args>(args)...);
            }

            template <typename... Args>
            void info(const char *file, int line, const char *func, std::format_string<Args...> fmt, Args &&...args)
            {
                if (m_log_level > INFO)
                {
                    return;
                }
                log(INFO, file, line, func, fmt, std::forward<Args>(args)...);
            }

            template <typename... Args>
            void warn(const char *file, int line, const char *func, std::format_string<Args...> fmt, Args &&...args)
            {
                if (m_log_level > WARN)
                {
                    return;
                }
                log(WARN, file, line, func, fmt, std::forward<Args>(args)...);
            }

            template <typename... Args>
            void error(const char *file, int line, const char *func, std::format_string<Args...> fmt, Args &&...args)
            {
                if (m_log_level > ERROR)
                {
                    return;
                }
                log(ERROR, file, line, func, fmt, std::forward<Args>(args)...);
            }

            template <typename... Args>
            void fatal(const char *file, int line, const char *func, std::format_string<Args...> fmt, Args &&...args)
            {
                if (m_log_level > FATAL)
                {
                    return;
                }
                log(FATAL, file, line, func, fmt, std::forward<Args>(args)...);
            }

        protected:
            template <typename... Args>
            void log(flag f, const char *file, int line, const char *func, std::format_string<Args...> fmt, Args &&...args)
            {
                std::lock_guard<std::mutex> lock(m_log_mutex);

                struct tm tm_now{};
                struct tm *ptm = nullptr;

                const std::time_t ticks = chrono::system_clock::to_time_t(chrono::system_clock::now());

                ptm = ::localtime_r(&ticks, &tm_now);
                if (ptm == nullptr)
                {
                    std::cerr << "log: localtime_r failed, message dropped" << '\n';
                    return;
                }

                // Check if log file needs rotation
                rotate_log_file(tm_now);

                if (m_fp == nullptr)
                {
                    std::cerr << "log: file not open, message dropped" << '\n';
                    m_reported_fp_null = true;
                    return;
                }

                // Get time for log entry
                char buf[32]{0};
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ptm);

                // using log file
                fprintf(m_fp, "%s  ", buf);
                fprintf(m_fp, "%s  ", s_flag[f]); // print flag
                fprintf(m_fp, "%s:%d %s  ", file, line, func);
                auto msg = std::format(fmt, std::forward<Args>(args)...);
                fprintf(m_fp, "%s\n", msg.c_str());
                fflush(m_fp);
                // free lock
            }

            void rotate_log_file(const struct tm &tm_now);
            void close_unlocked();

        protected:
            FILE *m_fp{nullptr};
            // Store base path for log files
            std::string m_base_path;
            // Track current hour for rotation
            struct tm m_last_tm{};
            // True once the current hour has been accounted for by a rotation attempt
            bool m_has_valid_tm{false};

            // Rate-limits the "file not open" notice to one per disabled period.
            bool m_reported_fp_null{false};

            std::atomic<int> m_log_level{0};

            std::mutex m_log_mutex;

            static const char *s_flag[FLAG_COUNT];
        };
    }
}

#define LOG_DEBUG(...) avant::log::logger::instance().debug(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(...) avant::log::logger::instance().info(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARN(...) avant::log::logger::instance().warn(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(...) avant::log::logger::instance().error(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_FATAL(...) avant::log::logger::instance().fatal(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define ASSERT_LOG_EXIT(EXPR) \
    if (!(EXPR))              \
    {                         \
        LOG_FATAL(#EXPR);     \
        exit(-1);             \
    }
