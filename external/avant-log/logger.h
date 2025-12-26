#pragma once
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <cstring>
#include <chrono>
#include <mutex>
#include <format>

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
             * @param log_level
             */

            void open(const string &log_file_base_path, const int log_level);

            /**
             * @brief close log file
             *
             */
            void close();

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

                if (m_fp == nullptr)
                {
                    printf("open log file failed: m_fp==nullptr\n");
                    exit(1);
                }

                std::time_t ticks = chrono::system_clock::to_time_t(chrono::system_clock::now());

                // Check if log file needs rotation
                rotate_log_file(ticks);

                // Get time for log enrty
                struct tm *ptm = std::localtime(&ticks);
                char buf[32];
                memset(buf, 0, sizeof(buf));
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ptm);

                // using log file
                fprintf(m_fp, "%s  ", buf);
                fprintf(m_fp, "%s  ", s_flag[f]); // print flag
                fprintf(m_fp, "%s:%d %s  ", file, line, func);
                auto msg = std::format(fmt, std::forward<Args>(args)...);
                fprintf(m_fp, "%s\r\n", msg.c_str());

                // free lock
                fflush(m_fp);
            }

            void rotate_log_file(std::time_t ticks);

        protected:
            FILE *m_fp{nullptr};
            // Store base path for log files
            std::string m_base_path;
            // Track current hour for rotation
            struct tm m_last_tm;
            bool m_has_valid_tm{false};

            int m_log_level{0};

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
