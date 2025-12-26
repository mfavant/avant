#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include "logger.h"

using namespace avant::log;

const char *logger::s_flag[FLAG_COUNT] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

logger::logger() : m_fp(nullptr), m_has_valid_tm(false)
{
    printf("logger::logger()\n");
    memset(&m_last_tm, 0, sizeof(m_last_tm));
}

logger::~logger()
{
    printf("logger::~logger()\n");
    close();
}

void logger::open(const string &log_file_base_path, const int log_level)
{
    if (DEBUG > log_level || log_level > FLAG_COUNT)
    {
        m_log_level = DEBUG;
    }
    else
    {
        m_log_level = log_level;
    }

    printf("logger::open(%s)\n", log_file_base_path.c_str());
    close();
    m_base_path = log_file_base_path;
    // Ensure the directory exists
    mkdir(log_file_base_path.c_str(), 0755);
    // Open the initial log file
    std::time_t ticks = chrono::system_clock::to_time_t(chrono::system_clock::now());
    rotate_log_file(ticks);
}

void logger::close()
{
    if (m_fp != nullptr)
    {
        fclose(m_fp);
        m_fp = nullptr;
    }
    m_has_valid_tm = false;
    memset(&m_last_tm, 0, sizeof(m_last_tm));
}

void logger::rotate_log_file(std::time_t ticks)
{
    // Get current time
    struct tm *ptm = std::localtime(&ticks);
    bool need_rotate = false;

    // Check if hours has changed
    if (!m_has_valid_tm || m_fp == nullptr)
    {
        need_rotate = true;
    }
    else if (ptm->tm_year != m_last_tm.tm_year ||
             ptm->tm_mon != m_last_tm.tm_mon ||
             ptm->tm_mday != m_last_tm.tm_mday ||
             ptm->tm_hour != m_last_tm.tm_hour)
    {
        need_rotate = true;
    }

    if (!need_rotate)
    {
        return; // No Need to rotate
    }

    // Close existing file
    close();

    // Generate new log file path
    char time_buf[32];
    memset(time_buf, 0, sizeof(time_buf));
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H", ptm); // Format: YYYY-MM-DD_HH

    // Generate new log file path
    std::string log_file_path = m_base_path + "/" + time_buf + ".log";

    m_fp = fopen(log_file_path.c_str(), "a+");
    if (m_fp == nullptr)
    {
        printf("open log file failed: %s\n", log_file_path.c_str());
        exit(1);
    }

    // Update last time
    m_last_tm = *ptm;
    m_has_valid_tm = true;
}

void logger::debug(const char *file, int line, const char *func, const char *format, ...)
{
    if (m_log_level > DEBUG)
    {
        return;
    }
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(DEBUG, file, line, func, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::info(const char *file, int line, const char *func, const char *format, ...)
{
    if (m_log_level > INFO)
    {
        return;
    }
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(INFO, file, line, func, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::warn(const char *file, int line, const char *func, const char *format, ...)
{
    if (m_log_level > WARN)
    {
        return;
    }
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(WARN, file, line, func, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::error(const char *file, int line, const char *func, const char *format, ...)
{
    if (m_log_level > ERROR)
    {
        return;
    }
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(ERROR, file, line, func, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::fatal(const char *file, int line, const char *func, const char *format, ...)
{
    if (m_log_level > FATAL)
    {
        return;
    }
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(FATAL, file, line, func, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::log(flag f, const char *file, int line, const char *func, const char *format, va_list arg_ptr)
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
    vfprintf(m_fp, format, arg_ptr); // formating print
    fprintf(m_fp, "\r\n");
    // free lock
    fflush(m_fp);
}
