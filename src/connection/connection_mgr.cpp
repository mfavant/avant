#include "connection/connection_mgr.h"

using avant::connection::connection_mgr;
using namespace avant::connection;

connection_mgr::connection_mgr()
{
}

connection_mgr::~connection_mgr()
{
    if (connection_pool)
    {
        delete[] connection_pool;
    }
}

int connection_mgr::init(size_t pool_max_size)
{
    connection *connection_arr = new (std::nothrow) connection[pool_max_size + 1];
    if (!connection_arr)
    {
        return -1;
    }
    connection_pool = connection_arr;
    for (size_t i = 0; i < pool_max_size + 1; i++)
    {
        unused_set.insert(i);
    }
    return 0;
}

int connection_mgr::alloc_connection(int fd, uint64_t gid)
{
    if (fd2gid.find(fd) != fd2gid.end())
    {
        return -1;
    }
    if (gid2fd.find(gid) != gid2fd.end())
    {
        return -2;
    }
    if (gid2index.find(gid) != gid2index.end())
    {
        return -3;
    }
    if (unused_set.empty())
    {
        return -4;
    }
    auto iter = unused_set.begin();
    if (iter == unused_set.end())
    {
        return -5;
    }
    size_t new_index = *iter;
    unused_set.erase(iter);

    using_set.insert(new_index);
    fd2gid[fd] = gid;
    gid2fd[gid] = fd;
    gid2index[gid] = new_index;

    connection_pool[new_index].on_alloc();

    return 0;
}

int connection_mgr::release_connection(int fd)
{
    auto fd2gid_iter = fd2gid.find(fd);
    if (fd2gid_iter == fd2gid.end())
    {
        return -1;
    }
    auto gid2fd_iter = gid2fd.find(fd2gid_iter->second);
    if (gid2fd_iter == gid2fd.end())
    {
        return -2;
    }
    auto gid2index_iter = gid2index.find(fd2gid_iter->second);
    if (gid2index_iter == gid2index.end())
    {
        return -3;
    }
    size_t index = gid2index_iter->second;
    fd2gid.erase(fd2gid_iter);
    gid2fd.erase(gid2fd_iter);
    gid2index.erase(gid2index_iter);
    using_set.erase(index);
    unused_set.insert(index);

    connection_pool[index].on_release();
    return 0;
}