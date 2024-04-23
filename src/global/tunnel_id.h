#pragma once
#include <memory>
#include <cstdint>
#include "proto_res/proto_tunnel.pb.h"

namespace avant::global
{
    class tunnel_id
    {
    public:
        static int init(size_t worker_cnt);

        static inline tunnel_id &get()
        {
            return *tunnel_id::ptr.get();
        }

        // worker_idx begin from zero
        inline int get_worker_tunnel_id(int worker_idx) const
        {
            return ProtoTunnelID::PROTO_TUNNEL_WORKER_MIN + worker_idx;
        }

        inline int get_worker_tunnel_id_min() const
        {
            return ProtoTunnelID::PROTO_TUNNEL_WORKER_MIN;
        }

        inline int get_worker_tunnel_id_max() const
        {
            return m_worker_cnt - 1;
        }

        inline int get_main_tunnel_id() const
        {
            return ProtoTunnelID::PROTO_TUNNEL_ID_MAIN;
        }

        inline int get_other_tunnel_id() const
        {
            return ProtoTunnelID::PROTO_TUNNEL_ID_OTHER;
        }

        inline int hash_gid_2_worker_tunnel_id(uint64_t gid)
        {
            return gid % m_worker_cnt;
        }

    private:
        size_t m_worker_cnt;
        static std::shared_ptr<tunnel_id> ptr;
    };
}
