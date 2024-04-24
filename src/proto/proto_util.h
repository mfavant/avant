#pragma once
#include <cstdint>
#include "proto_res/proto_cmd.pb.h"
#include "proto_res/proto_message_head.pb.h"

namespace avant::proto
{
    uint64_t ton64(uint64_t num);
    uint64_t toh64(uint64_t num);

    template <typename T>
    std::string &pack_package(std::string &data, const T &t, ProtoCmd cmd)
    {
        data.clear();
        ProtoPackage message;

        std::string body_str;
        body_str.resize(t.ByteSizeLong());
        t.SerializeToString(&body_str);

        message.set_cmd(cmd);
        message.set_protocol(body_str);

        message.SerializeToString(&data);
        uint64_t len = ton64(data.size());
        data.insert(0, (char *)&len, sizeof(len));
        return data;
    }

    template <typename T>
    ProtoPackage &pack_package(ProtoPackage &package, const T &t, ProtoCmd cmd)
    {
        std::string body_str;
        body_str.resize(t.ByteSizeLong());
        t.SerializeToString(&body_str);

        package.set_cmd(cmd);
        package.set_protocol(body_str);
        return package;
    }

    template <typename T>
    bool parse(T &t, const ProtoPackage &package)
    {
        return t.ParseFromArray(package.protocol().c_str(), package.protocol().size());
    }

    std::string &pack_package(std::string &data, const ProtoPackage &package);
};