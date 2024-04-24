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

std::string &proto::pack_package(std::string &data, const ProtoPackage &package)
{
    data.clear();
    package.SerializeToString(&data);
    uint64_t len = ton64(data.size());
    data.insert(0, (char *)&len, sizeof(len));
    return data;
}