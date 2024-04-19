#pragma once
#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <cstdlib>
#include <string>
#include <cstring>

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
             * @brief open log file
             *
             * @param log_file_path
             */

            void open(const string &log_file_path);

            /**
             * @brief close log file
             *
             */
            void close();

            /**
             * @brief out log in debug
             *
             * @param file content
             * @param line line number
             * @param format
             * @param ... param
             */
            void debug(const char *file, int line, const char *format, ...);

            /**
             * @brief out log in info
             *
             * @param file content
             * @param line line number
             * @param format
             * @param ... param
             */
            void info(const char *file, int line, const char *format, ...);

            /**
             * @brief out log in warn
             *
             * @param file content
             * @param line line number
             * @param format
             * @param ... param
             */
            void warn(const char *file, int line, const char *format, ...);

            /**
             * @brief out log in error
             *
             * @param file content
             * @param line line number
             * @param format
             * @param ... param
             */
            void error(const char *file, int line, const char *format, ...);

            /**
             * @brief out log in fatlal
             *
             * @param file content
             * @param line line number
             * @param format
             * @param ... param
             */
            void fatal(const char *file, int line, const char *format, ...);

        protected:
            void log(flag f, const char *file, int line, const char *format, va_list arg_ptr);

        protected:
            FILE *m_fp{nullptr};
            static const char *s_flag[FLAG_COUNT];
        };
    }
}

#define LOG_DEBUG(...) avant::log::logger::instance().debug(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) avant::log::logger::instance().info(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) avant::log::logger::instance().warn(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) avant::log::logger::instance().error(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) avant::log::logger::instance().fatal(__FILE__, __LINE__, __VA_ARGS__)