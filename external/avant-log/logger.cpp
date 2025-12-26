#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <iostream>
#include <format>
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
    std::cout << "logger::logger()" << std::endl;
    memset(&m_last_tm, 0, sizeof(m_last_tm));
}

logger::~logger()
{
    std::cout << "logger::~logger()" << std::endl;
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

    std::cout << std::format("logger::open({})", log_file_base_path) << std::endl;

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
        std::cout << std::format("open log file failed: {}", log_file_path) << std::endl;
        exit(1);
    }

    // Update last time
    m_last_tm = *ptm;
    m_has_valid_tm = true;
}
