#pragma once

#include <atomic>
#include <unistd.h>

namespace avant::system
{
    inline std::atomic<bool> g_system_alive{false};   // false => the handlers must no-op
    inline std::atomic<bool> g_daemon{false};         // mirrors system::m_daemon
    inline std::atomic<bool> g_stop_request{false};   // set by SIGINT / SIGTERM
    inline std::atomic<bool> g_reload_request{false}; // set by SIGUSR1
    inline std::atomic<int> g_signal_pipe_wr{-1};     // self-pipe write end, -1 if none

    // Record a stop/reload request and wake the event loop.
    inline void async_signal_notify(bool reload)
    {
        (reload ? g_reload_request : g_stop_request).store(true, std::memory_order_release);
        const int fd = g_signal_pipe_wr.load(std::memory_order_acquire);
        if (fd >= 0)
        {
            const char byte = reload ? 'R' : 'S';
            (void)::write(fd, &byte, 1);
        }
    }
} // namespace avant::system
