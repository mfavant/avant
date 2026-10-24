#include "global/tunnel_id.h"

using namespace avant::global;

namespace avant::global
{
    static std::shared_ptr<tunnel_id> g_tunnel_id_ptr;
}

int tunnel_id::init(int worker_cnt)
{
    if (g_tunnel_id_ptr)
    {
        return -1;
    }
    tunnel_id *new_tunnel_id_obj = new (std::nothrow) tunnel_id;
    if (!new_tunnel_id_obj)
    {
        return -2;
    }
    g_tunnel_id_ptr.reset(new_tunnel_id_obj);
    g_tunnel_id_ptr->m_worker_cnt = worker_cnt;
    return 0;
}

tunnel_id &tunnel_id::get()
{
    assert(g_tunnel_id_ptr);
    return *g_tunnel_id_ptr.get();
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
