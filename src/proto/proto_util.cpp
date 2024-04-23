#include "proto/proto_util.h"
#include <endian.h>

using namespace avant;

uint64_t proto::ton64(uint64_t num)
{
    return ::htobe64(num);
}

uint64_t proto::toh64(uint64_t num)
{
    return ::be64toh(num);
}