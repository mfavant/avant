#include "global/tunnel_id.h"

using namespace avant::global;

std::shared_ptr<tunnel_id> tunnel_id::ptr;

int tunnel_id::init(size_t worker_cnt)
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
