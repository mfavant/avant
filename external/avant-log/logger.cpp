#include <chrono>

#include "logger.h"

using namespace avant::log;

const char *logger::s_flag[FLAG_COUNT] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
};

logger::logger() : m_fp(nullptr)
{
}

logger::~logger()
{
    close();
}

void logger::open(const string &log_file_path)
{
    close();
    m_fp = fopen(log_file_path.c_str(), "a+");
    if (m_fp == nullptr)
    {
        printf("open log file failed: %s\n", log_file_path.c_str());
        exit(1);
    }
}

void logger::close()
{
    if (m_fp != nullptr)
    {
        fclose(m_fp);
        m_fp = nullptr;
    }
}

void logger::debug(const char *file, int line, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(DEBUG, file, line, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::info(const char *file, int line, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(INFO, file, line, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::warn(const char *file, int line, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(WARN, file, line, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::error(const char *file, int line, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(ERROR, file, line, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::fatal(const char *file, int line, const char *format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    log(FATAL, file, line, format, arg_ptr);
    va_end(arg_ptr);
}

void logger::log(flag f, const char *file, int line, const char *format, va_list arg_ptr)
{
    if (m_fp == nullptr)
    {
        printf("open log file failed: m_fp==nullptr\n");
        exit(1);
    }
    // get time now
    std::time_t ticks = chrono::system_clock::to_time_t(chrono::system_clock::now());
    struct tm *ptm = std::localtime(&ticks);
    char buf[32];
    memset(buf, 0, sizeof(buf));
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ptm);
    // ensure safe using file in different thread
    flockfile(m_fp); // print time
    // using log file
    fprintf(m_fp, "%s  ", buf);
    fprintf(m_fp, "%s  ", s_flag[f]); // print flag
    fprintf(m_fp, "%s:%d  ", file, line);
    vfprintf(m_fp, format, arg_ptr); // formating print
    fprintf(m_fp, "\r\n");
    // free lock
    fflush(m_fp);
    funlockfile(m_fp);
}
