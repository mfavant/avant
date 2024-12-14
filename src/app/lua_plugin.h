#pragma once
#include <string>
#include "proto/proto_util.h"

extern "C"
{
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

namespace avant::app
{
    class lua_plugin
    {
    public:
        lua_plugin();
        ~lua_plugin();

        void on_main_init(const std::string &lua_dir, const int worker_cnt);

        void real_on_main_init(bool is_hot);
        void on_main_stop(bool is_hot);
        void on_main_tick();
        void exe_OnMainInit(bool is_hot);
        void exe_OnMainStop(bool is_hot);
        void exe_OnMainTick();

        void on_worker_init(int worker_idx, bool is_hot);
        void on_worker_stop(int worker_idx, bool is_hot);
        void on_worker_tick(int worker_idx);
        void exe_OnWorkerInit(int worker_idx, bool is_hot);
        void exe_OnWorkerStop(int worker_idx, bool is_hot);
        void exe_OnWorkerTick(int worker_idx);

        static void exe_OnLuaVMRecvMessage(lua_State *lua_state, int cmd, const google::protobuf::Message &package);

        void on_other_init(bool is_hot);
        void on_other_stop(bool is_hot);
        void on_other_tick();
        void exe_OnOtherInit(bool is_hot);
        void exe_OnOtherStop(bool is_hot);
        void exe_OnOtherTick();

        void main_mount();
        void worker_mount(int worker_idx);
        void other_mount();

        void reload();

    public:
        static int Logger(lua_State *lua_state);
        static int Lua2Protobuf(lua_State *lua_state);

    public:
        static void protobuf2lua(lua_State *L, const google::protobuf::Message &package);
        static void lua2protobuf(lua_State *L, google::protobuf::Message &package);

    private:
        void free_main_lua();
        void free_worker_lua();
        void free_other_lua();
        void free_worker_lua(int worker_idx);

    private:
        lua_State *lua_state{nullptr};
        bool lua_state_be_reload{false};

        lua_State **worker_lua_state{nullptr};
        bool *worker_lua_state_be_reload{nullptr};
        int worker_lua_cnt{0};

        lua_State *other_lua_state{nullptr};
        bool other_lua_state_be_reload{false};

        std::string lua_dir;
    };
}