#pragma once
#include <string>

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
        void on_main_stop();
        void on_main_tick();
        void exe_OnMainInit();
        void exe_OnMainStop();
        void exe_OnMainTick();

        void on_worker_init(int worker_idx);
        void on_worker_stop(int worker_idx);
        void on_worker_tick(int worker_idx);
        void exe_OnWorkerInit(int worker_idx);
        void exe_OnWorkerStop(int worker_idx);
        void exe_OnWorkerTick(int worker_idx);

        void on_other_init();
        void on_other_stop();
        void on_other_tick();
        void exe_OnOtherInit();
        void exe_OnOtherStop();
        void exe_OnOtherTick();

        void mount();

    public:
        static int Logger(lua_State *lua_state);

    private:
        lua_State *lua_state{nullptr};
        lua_State **worker_lua_state{nullptr};
        int worker_lua_cnt{0};
        lua_State *other_lua_state{nullptr};
    };
}