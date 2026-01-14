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

        int alloc_connection(int fd, uint64_t gid);
        int release_connection(int fd);
        uint64_t size();

        [[nodiscard]] connection *get_conn(int fd);
        [[nodiscard]] connection *get_conn_by_gid(uint64_t gid);
        [[nodiscard]] connection *get_conn_by_idx(uint64_t idx);

        connection *connection_pool{nullptr};
        std::unordered_map<int, uint64_t> fd2gid{};
        std::unordered_map<uint64_t, int> gid2fd{};
        std::unordered_map<uint64_t, size_t> gid2index{};
        std::unordered_set<size_t> using_set{};
        std::unordered_set<size_t> unused_set{};
    };
}