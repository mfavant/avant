#pragma once
#include <memory>
#include <cstdint>
#include <cassert>
#include <vector>
#include "proto_res/proto_tunnel.pb.h"

namespace avant::global
{
    class tunnel_id
    {
    public:
        static int init(int worker_cnt);
        static tunnel_id &get();

        // worker_idx begin from zero
        inline int get_worker_tunnel_id(int worker_idx) const
        {
            return ProtoTunnelID::PROTO_TUNNEL_WORKER_MIN + worker_idx;
        }

        inline int get_worker_idx_by_tunnel_id(int tunnel_id) const
        {
            return tunnel_id - ProtoTunnelID::PROTO_TUNNEL_WORKER_MIN;
        }

        void get_worker_all_tunnel_id(std::vector<int> &vec_worker_id) const;

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

        inline int hash_gid_2_worker_tunnel_id(uint64_t gid) const
        {
            return get_worker_tunnel_id(gid & 0x1FF);
        }

        bool is_tunnel_id(int id) const;

        inline bool is_worker_tunnel_id(int id) const
        {
            return (get_worker_tunnel_id_min() <= id && id <= get_worker_tunnel_id_max());
        }

    private:
        int m_worker_cnt;
        static std::shared_ptr<tunnel_id> ptr;
    };
}
