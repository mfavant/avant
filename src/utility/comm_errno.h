#pragma once
#include <errno.h>

namespace avant::utility::comm_errno
{
    class comm_errno
    {
    public:
        static constexpr int COMM_ERRNO_EINTR = EINTR;
        static constexpr int COMM_ERRNO_EAGAIN = EAGAIN;
        static constexpr int COMM_ERRNO_EWOULDBLOCK = EWOULDBLOCK;
    };
};
