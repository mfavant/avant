#include "app/lua_plugin.h"
#include <avant-log/logger.h>
#include <string>
#include <cstdlib>

using namespace avant::app;

lua_plugin::lua_plugin() : lua_state(nullptr)
{
}

lua_plugin::~lua_plugin()
{
    if (lua_state)
    {
        lua_close(lua_state);
        lua_state = nullptr;
    }
    if (worker_lua_state)
    {
        for (int i = 0; i < worker_lua_cnt; i++)
        {
            if (worker_lua_state[i])
            {
                lua_close(worker_lua_state[i]);
                worker_lua_state[i] = nullptr;
            }
        }
        delete[] worker_lua_state;
    }
    if (other_lua_state)
    {
        lua_close(other_lua_state);
        other_lua_state = nullptr;
    }
}

void lua_plugin::on_main_init(const std::string &lua_dir, const int worker_cnt)
{
    // init main lua vm
    {
        lua_state = luaL_newstate();
        luaL_openlibs(lua_state);
        std::string filename = lua_dir + "/init.lua";
        int isok = luaL_dofile(lua_state, filename.data());

        if (isok == LUA_OK)
        {
            LOG_ERROR("main init.lua load succ");
        }
        else
        {
            LOG_ERROR("main init.lua load failed, %s", lua_tostring(lua_state, -1));
            exit(-1);
        }
    }

    // init worker lua vm
    {
        worker_lua_cnt = worker_cnt;
        worker_lua_state = new lua_State *[worker_cnt];
        for (int i = 0; i < worker_cnt; i++)
        {
            worker_lua_state[i] = luaL_newstate();
            luaL_openlibs(worker_lua_state[i]);
            std::string filename = lua_dir + "/init.lua";
            int isok = luaL_dofile(worker_lua_state[i], filename.data());
            if (isok == LUA_OK)
            {
                LOG_ERROR("worker vm[%d] init.lua load succ", i);
            }
            else
            {
                LOG_ERROR("main worker vm[%d] init.lua load failed, %s", i, lua_tostring(worker_lua_state[i], -1));
                exit(-1);
            }
        }
    }

    // init other vm
    {
        other_lua_state = luaL_newstate();
        luaL_openlibs(other_lua_state);
        std::string filename = lua_dir + "/init.lua";
        int isok = luaL_dofile(other_lua_state, filename.data());

        if (isok == LUA_OK)
        {
            LOG_ERROR("other init.lua load succ");
        }
        else
        {
            LOG_ERROR("other init.lua load failed, %s", lua_tostring(other_lua_state, -1));
            exit(-1);
        }
    }

    mount();
    exe_OnMainInit();
}

void lua_plugin::on_main_stop()
{
    exe_OnMainStop();
}

void lua_plugin::on_main_tick()
{
    exe_OnMainTick();
}

void lua_plugin::on_worker_init(int worker_idx)
{
    exe_OnWorkerInit(worker_idx);
}

void lua_plugin::on_worker_stop(int worker_idx)
{
    exe_OnWorkerStop(worker_idx);
}

void lua_plugin::on_worker_tick(int worker_idx)
{
    exe_OnWorkerTick(worker_idx);
}

void lua_plugin::exe_OnMainInit()
{
    lua_getglobal(lua_state, "OnInit");
    int isok = lua_pcall(lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnMainInit failed %s", lua_tostring(lua_state, -1));
    }
}

void lua_plugin::exe_OnMainStop()
{
    lua_getglobal(lua_state, "OnMainStop");
    int isok = lua_pcall(lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnMainStop failed %s", lua_tostring(lua_state, -1));
    }
}

void lua_plugin::exe_OnMainTick()
{
    lua_getglobal(lua_state, "OnMainTick");
    static int isok = 0;
    isok = lua_pcall(lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnMainTick failed %s", lua_tostring(lua_state, -1));
    }
}

void lua_plugin::exe_OnWorkerInit(int worker_idx)
{
    lua_State *lua_ptr = worker_lua_state[worker_idx];
    lua_getglobal(lua_ptr, "OnWorkerInit");
    lua_pushinteger(lua_ptr, worker_idx);
    int isok = lua_pcall(lua_ptr, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnWorkerInit failed %s", lua_tostring(lua_ptr, -1));
    }
}

void lua_plugin::exe_OnWorkerStop(int worker_idx)
{
    lua_State *lua_ptr = worker_lua_state[worker_idx];
    lua_getglobal(lua_ptr, "OnWorkerStop");
    lua_pushinteger(lua_ptr, worker_idx);
    int isok = lua_pcall(lua_ptr, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnWorkerStop failed %s", lua_tostring(lua_ptr, -1));
    }
}

void lua_plugin::exe_OnWorkerTick(int worker_idx)
{
    lua_State *lua_ptr = worker_lua_state[worker_idx];
    lua_getglobal(lua_ptr, "OnWorkerTick");
    lua_pushinteger(lua_ptr, worker_idx);
    int isok = lua_pcall(lua_ptr, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnWorkerTick failed %s", lua_tostring(lua_ptr, -1));
    }
}

void lua_plugin::on_other_init()
{
    exe_OnOtherInit();
}

void lua_plugin::on_other_stop()
{
    exe_OnOtherStop();
}

void lua_plugin::on_other_tick()
{
    exe_OnOtherTick();
}

void lua_plugin::exe_OnOtherInit()
{
    lua_getglobal(other_lua_state, "OnOtherInit");
    static int isok = 0;
    isok = lua_pcall(other_lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnOtherInit failed %s", lua_tostring(other_lua_state, -1));
    }
}

void lua_plugin::exe_OnOtherStop()
{
    lua_getglobal(other_lua_state, "OnOtherStop");
    static int isok = 0;
    isok = lua_pcall(other_lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnOtherStop failed %s", lua_tostring(other_lua_state, -1));
    }
}

void lua_plugin::exe_OnOtherTick()
{
    lua_getglobal(other_lua_state, "OnOtherTick");
    static int isok = 0;
    isok = lua_pcall(other_lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnOtherTick failed %s", lua_tostring(other_lua_state, -1));
    }
}

void lua_plugin::mount()
{
    static luaL_Reg main_lulibs[] = {
        {"Logger", Logger},
        {NULL, NULL}};
    {
        // mount main lua vm
        luaL_newlib(lua_state, main_lulibs);
        lua_setglobal(lua_state, "avant");
    }

    static luaL_Reg worker_lulibs[] = {
        {"Logger", Logger},
        {NULL, NULL}};
    {
        for (int i = 0; i < worker_lua_cnt; i++)
        {
            luaL_newlib(worker_lua_state[i], worker_lulibs);
            lua_setglobal(worker_lua_state[i], "avant");
        }
    }

    static luaL_Reg other_lulibs[] = {
        {"Logger", Logger},
        {NULL, NULL}};
    {
        luaL_newlib(other_lua_state, other_lulibs);
        lua_setglobal(other_lua_state, "avant");
    }
}

int lua_plugin::Logger(lua_State *lua_state)
{
    int num = lua_gettop(lua_state);
    if (num != 1)
    {
        lua_pushinteger(lua_state, -1);
        return -1;
    }
    if (lua_isstring(lua_state, 1) == 0)
    {
        lua_pushinteger(lua_state, -1);
        return -1;
    }
    size_t len = 0;
    const char *type = lua_tolstring(lua_state, 1, &len);
    std::string str(type, len);
    LOG_ERROR("lua => %s", str.data());
    lua_pushinteger(lua_state, 0);
    return 0;
}
