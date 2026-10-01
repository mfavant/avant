#include <chrono>
#include <filesystem>
#include <system_error>
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

logger::logger()
{
    std::cout << "logger::logger()" << std::endl;
}

logger::~logger()
{
    std::cout << "logger::~logger()" << std::endl;
    std::lock_guard<std::mutex> lock(m_log_mutex);
    close_unlocked();
}

static bool mkdir_p(const std::string &path)
{
    if (path.empty())
    {
        return true;
    }

    std::error_code ec;
    if (std::filesystem::is_directory(path, ec) && !ec)
    {
        std::cerr << path << " is directory" << std::endl;
        return true;
    }
    ec.clear();

    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        std::cerr << path << " create directories failed" << std::endl;
        return false;
    }

    ec.clear();
    return std::filesystem::is_directory(path, ec) && !ec;
}

int logger::open(const std::string &log_file_base_path, const int log_level)
{
    const int clamped = (log_level < DEBUG || log_level >= FLAG_COUNT) ? DEBUG : log_level;

    std::cout << std::format("logger::open({})", log_file_base_path) << '\n';

    std::lock_guard<std::mutex> lock(m_log_mutex);

    m_log_level.store(clamped);
    close_unlocked();

    m_base_path = log_file_base_path;
    if (!mkdir_p(m_base_path))
    {
        std::cerr << std::format("logger::open: failed to create log directory '{}'", m_base_path) << '\n';
        return -1;
    }

    // Open the initial (hourly) log file.
    const std::time_t ticks = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm tm_now{};
    if (::localtime_r(&ticks, &tm_now) != nullptr)
    {
        rotate_log_file(tm_now);
    }

    if (m_fp == nullptr)
    {
        std::cerr << "logger::open: could not open the initial log file" << '\n';
        return -1;
    }

    return 0;
}

void logger::close_unlocked()
{
    if (m_fp != nullptr)
    {
        std::fclose(m_fp);
        m_fp = nullptr;
    }
    m_has_valid_tm = false;
    m_last_tm = {};
    m_reported_fp_null = false;
}

void logger::rotate_log_file(const struct tm &tm_now)
{
    // Check if hours has changed. (m_fp == nullptr alone must NOT trigger a
    // rotation: a failed rotation keeps m_fp null but has already accounted
    // for the hour, so this bounds the fopen retries to one per hour.)
    const bool need_rotate = !m_has_valid_tm ||
                             tm_now.tm_year != m_last_tm.tm_year ||
                             tm_now.tm_mon != m_last_tm.tm_mon ||
                             tm_now.tm_mday != m_last_tm.tm_mday ||
                             tm_now.tm_hour != m_last_tm.tm_hour;

    if (!need_rotate)
    {
        return; // No need to rotate
    }

    // Close existing file
    close_unlocked();

    // Generate new log file path
    char time_buf[32]{0};
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d_%H", &tm_now); // Format: YYYY-MM-DD_HH

    const std::string log_file_path = m_base_path + "/" + time_buf + ".log";

    m_fp = std::fopen(log_file_path.c_str(), "a+");
    if (m_fp == nullptr)
    {
        std::cerr << std::format("open log file failed: {}", log_file_path) << '\n';
        // Account for this hour so the next log() call drops the message
        // silently instead of retrying fopen; the next hour retries.
        m_last_tm = tm_now;
        m_has_valid_tm = true;
        return;
    }

    // Update last time
    m_last_tm = tm_now;
    m_has_valid_tm = true;
}
