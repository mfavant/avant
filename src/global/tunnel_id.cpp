#include "global/tunnel_id.h"

using namespace avant::global;

std::shared_ptr<tunnel_id> tunnel_id::ptr;

int tunnel_id::init(int worker_cnt)
{
    if (tunnel_id::ptr)
    {
        return -1;
    }
    tunnel_id *new_tunnel_id_obj = new (std::nothrow) tunnel_id;
    if (!new_tunnel_id_obj)
    {
        return -2;
    }
    tunnel_id::ptr.reset(new_tunnel_id_obj);
    tunnel_id::ptr->m_worker_cnt = worker_cnt;
    return 0;
}

tunnel_id &tunnel_id::get()
{
    assert(tunnel_id::ptr);
    return *tunnel_id::ptr.get();
}

void tunnel_id::get_worker_all_tunnel_id(std::vector<int> &vec_worker_id) const
{
    for (int i = get_worker_tunnel_id_min(); i <= get_worker_tunnel_id_max(); ++i)
    {
        vec_worker_id.push_back(i);
    }
}

bool tunnel_id::is_tunnel_id(int id) const
{
    if (get_worker_tunnel_id_min() <= id && id <= get_worker_tunnel_id_max())
    {
        return true;
    }
    if (get_other_tunnel_id() == id)
    {
        return true;
    }
    if (get_main_tunnel_id() == id)
    {
        return true;
    }
    return false;
}
