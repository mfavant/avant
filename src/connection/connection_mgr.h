#pragma once
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "connection/connection.h"

namespace avant::connection
{
    class connection_mgr
    {
    public:
        connection_mgr();
        ~connection_mgr();
        int init(size_t pool_max_size);
        connection *connection_pool{nullptr};
        int alloc_connection(int fd, uint64_t gid);
        int release_connection(int fd);
        avant::connection::connection *get_conn(int fd);

        std::unordered_map<int, uint64_t> fd2gid{};
        std::unordered_map<uint64_t, int> gid2fd{};
        std::unordered_map<uint64_t, size_t> gid2index{};
        std::unordered_set<size_t> using_set{};
        std::unordered_set<size_t> unused_set{};
    };
};