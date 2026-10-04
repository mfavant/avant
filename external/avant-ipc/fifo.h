#pragma once

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace avant
{
    namespace ipc
    {
        // Full-duplex pipe channel between exactly two peers, built from a pair
        // of POSIX fifos:
        //   - peer A writes to a2b_path and reads from b2a_path
        //   - peer B the other way round
        //
        // Behaviour notes:
        // - init() opens both fifos with O_RDWR | O_NONBLOCK, so it NEVER blocks
        //   waiting for the other peer to open its end; the peer may call init()
        //   later. Direction is a convention (see get_read_fd/get_write_fd), the
        //   fds themselves allow both operations.
        // - Because the write end is kept open by ourselves (O_RDWR), a peer
        //   closing its side does NOT surface as read()==0; use the close of the
        //   channel (EPIPE on write) or an out-of-band signal for shutdown.
        // - Both fds are non-blocking: write() returns -1 (errno EAGAIN/EWOULDBLOCK)
        //   when the pipe buffer is full; the caller should retry once the fd
        //   reports writable (e.g. from an event poller) instead of blocking.
        // - init() throws std::runtime_error on failure; the destructor unlinks
        //   the fifo paths and invokes destroy_callback.
        class fifo
        {
        public:
            enum iam
            {
                AUTH_A = 0,
                AUTH_B = 1,
                AUTH_NONE = 2
            };

            fifo(const std::string &a2b_path,
                 const std::string &b2a_path,
                 iam me,
                 std::function<void(fifo &)> destroy_callback) : a2b_path(a2b_path),
                                                                 b2a_path(b2a_path),
                                                                 me(me),
                                                                 destroy_callback(destroy_callback)
            {
            }

            ~fifo()
            {
                if (destroy_callback)
                {
                    destroy_callback(*this);
                }
                if (fd_a2b >= 0)
                {
                    ::close(fd_a2b);
                    ::unlink(a2b_path.c_str());
                    fd_a2b = -1;
                }
                if (fd_b2a >= 0)
                {
                    ::close(fd_b2a);
                    ::unlink(b2a_path.c_str());
                    fd_b2a = -1;
                }
            }

            void init()
            {
                if (a2b_path.empty())
                {
                    throw std::runtime_error("a2b_path empty");
                }
                if (b2a_path.empty())
                {
                    throw std::runtime_error("b2a_path empty");
                }
                if (me == AUTH_NONE)
                {
                    throw std::runtime_error("unknow me, it is not AUTH_A or AUTH_B");
                }

                int ret = mkfifo(a2b_path.c_str(), 0777);
                if (ret == -1 && errno != EEXIST)
                {
                    throw std::runtime_error(std::string("mkfifo a2b_path failed: ") + strerror(errno));
                }
                ret = mkfifo(b2a_path.c_str(), 0777);
                if (ret == -1 && errno != EEXIST)
                {
                    throw std::runtime_error(std::string("mkfifo b2a_path failed: ") + strerror(errno));
                }

                // O_RDWR | O_NONBLOCK: opening a fifo with a single direction
                // blocks until the other end opens; O_RDWR never blocks, so
                // init() cannot hang on a peer that is not up yet. Both peers
                // open the same two paths; `me` only decides which is read/write.
                fd_b2a = open(b2a_path.c_str(), O_RDWR | O_NONBLOCK);
                if (fd_b2a < 0)
                {
                    throw std::runtime_error(std::string("open b2a err: ") + strerror(errno));
                }
                fd_a2b = open(a2b_path.c_str(), O_RDWR | O_NONBLOCK);
                if (fd_a2b < 0)
                {
                    close(fd_b2a);
                    fd_b2a = -1;
                    throw std::runtime_error(std::string("open a2b err: ") + strerror(errno));
                }

                // fds are already O_NONBLOCK from open(); keep the contract explicit
                for (int fd : {fd_a2b, fd_b2a})
                {
                    int flags = fcntl(fd, F_GETFL, 0);
                    if (flags == -1 || 0 != fcntl(fd, F_SETFL, flags | O_NONBLOCK))
                    {
                        throw std::runtime_error("set nonblock fd err");
                    }
                }

                init_succ = true;
            }

            int get_write_fd()
            {
                if (this->me == AUTH_A)
                {
                    return fd_a2b;
                }
                else if (this->me == AUTH_B)
                {
                    return fd_b2a;
                }
                else
                {
                    throw std::runtime_error("unknow me, it is not AUTH_A or AUTH_B");
                }
            }

            int get_read_fd()
            {
                if (this->me == AUTH_A)
                {
                    return fd_b2a;
                }
                else if (this->me == AUTH_B)
                {
                    return fd_a2b;
                }
                else
                {
                    throw std::runtime_error("unknow me, it is not AUTH_A or AUTH_B");
                }
            }

            bool is_init_succ()
            {
                return init_succ;
            }

            // Write len bytes; loops until all bytes are in the pipe.
            // Returns len on success, or -1 (errno set: EAGAIN when the pipe
            // buffer is full, EPIPE when the peer vanished). Callers handling
            // backpressure should retry on EAGAIN once the write fd is writable.
            int write(const char *buffer, int len)
            {
                if (len < 0 || (buffer == nullptr && len > 0))
                {
                    errno = EINVAL;
                    return -1;
                }
                int total_written = 0;
                while (total_written < len)
                {
                    int written = ::write(get_write_fd(), buffer + total_written, len - total_written);
                    if (written == -1)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        // EAGAIN/EWOULDBLOCK (pipe full) or EPIPE (peer closed):
                        // do NOT spin; let the caller retry via its event loop.
                        return -1;
                    }
                    total_written += written;
                }
                return total_written;
            }

            // Read up to len bytes; returns the number of bytes read,
            // 0 when nothing was available (pipe drained / peer closed),
            // -1 on hard error (errno set).
            int recv(char *buffer, int len)
            {
                if (len < 0 || (buffer == nullptr && len > 0))
                {
                    errno = EINVAL;
                    return -1;
                }
                int total_read = 0;
                while (total_read < len)
                {
                    int read_bytes = ::read(get_read_fd(), buffer + total_read, len - total_read);
                    if (read_bytes == -1)
                    {
                        if (errno == EINTR)
                        {
                            continue;
                        }
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break; // pipe drained; wait for the next readable event
                        }
                        return -1;
                    }
                    else if (read_bytes == 0)
                    {
                        break; // peer closed the write end
                    }
                    total_read += read_bytes;
                }
                return total_read;
            }

        public:
            std::string get_a2b_path()
            {
                return a2b_path;
            }
            std::string get_b2a_path()
            {
                return b2a_path;
            }

        private:
            std::string a2b_path;
            std::string b2a_path;
            int fd_a2b{-1};
            int fd_b2a{-1};
            iam me{AUTH_NONE};
            bool init_succ{false};
            std::function<void(fifo &)> destroy_callback{nullptr};
        };
    } // namespace ipc
} // namespace avant
