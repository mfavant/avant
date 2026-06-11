#include "proto/proto_util.h"
#ifdef __linux__
#include <endian.h>
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#endif

using namespace avant;

uint64_t proto::ton64(uint64_t num)
{
#ifdef __linux__
    return ::htobe64(num);
#elif defined(__APPLE__)
    return OSSwapHostToBigInt64(num);
#endif
}

uint64_t proto::toh64(uint64_t num)
{
#ifdef __linux__
    return ::be64toh(num);
#elif defined(__APPLE__)
    return OSSwapBigToHostInt64(num);
#endif
}

uint64_t proto::toh64_from_buffer(const void *buffer)
{
    uint64_t num = 0;
    memcpy(&num, buffer, sizeof(num));
    return proto::toh64(num);
}

std::string &proto::pack_package(std::string &data, const ProtoPackage &package)
{
    data.clear();
    (void)package.SerializeToString(&data);
    uint64_t len = proto::ton64(data.size());
    data.insert(0, (char *)&len, sizeof(len));
    return data;
}