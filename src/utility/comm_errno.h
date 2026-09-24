#pragma once
#include <errno.h>

namespace avant::utility::comm_errno
{
    inline constexpr int COMM_ERRNO_EINTR = EINTR;
    inline constexpr int COMM_ERRNO_EAGAIN = EAGAIN;
    inline constexpr int COMM_ERRNO_EWOULDBLOCK = EWOULDBLOCK;
};
