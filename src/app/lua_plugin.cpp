#include "app/lua_plugin.h"
#include <avant-log/logger.h>
#include <string>
#include <cstdlib>
#include "proto/proto_util.h"
#include "proto_res/proto_lua.pb.h"
#include "utility/singleton.h"

using namespace avant::app;
using namespace avant::utility;

lua_plugin::lua_plugin()
{
}

lua_plugin::~lua_plugin()
{
    free_main_lua();
    free_worker_lua();
    free_other_lua();
}

void lua_plugin::free_main_lua()
{
    if (this->lua_state)
    {
        lua_close(this->lua_state);
        this->lua_state = nullptr;
    }
}

void lua_plugin::free_worker_lua()
{
    if (this->worker_lua_state)
    {
        for (int i = 0; i < this->worker_lua_cnt; i++)
        {
            free_worker_lua(i);
        }
        delete[] this->worker_lua_state;
    }
    if (this->worker_lua_state_be_reload)
    {
        delete[] this->worker_lua_state_be_reload;
    }
}

void lua_plugin::free_worker_lua(int worker_idx)
{
    if (this->worker_lua_state)
    {
        if (this->worker_lua_state[worker_idx])
        {
            lua_close(this->worker_lua_state[worker_idx]);
            this->worker_lua_state[worker_idx] = nullptr;
        }
    }
}

void lua_plugin::free_other_lua()
{
    if (this->other_lua_state)
    {
        lua_close(this->other_lua_state);
        this->other_lua_state = nullptr;
    }
}

void lua_plugin::reload()
{
    this->lua_state_be_reload = true;
    for (int i = 0; i < this->worker_lua_cnt; i++)
    {
        this->worker_lua_state_be_reload[i] = true;
    }
    this->other_lua_state_be_reload = true;
}

void lua_plugin::on_main_init(const std::string &lua_dir, const int worker_cnt)
{
    this->lua_dir = lua_dir;

    // init worker lua vm
    {
        this->worker_lua_cnt = worker_cnt;
        this->worker_lua_state = new lua_State *[this->worker_lua_cnt];
        this->worker_lua_state_be_reload = new bool[this->worker_lua_cnt];
        for (int i = 0; i < this->worker_lua_cnt; i++)
        {
            this->worker_lua_state[i] = nullptr;
            this->worker_lua_state_be_reload[i] = false;
        }
    }

    real_on_main_init(false);
}

void lua_plugin::real_on_main_init(bool is_hot)
{
    // init main lua vm
    {
        this->lua_state = luaL_newstate();
        luaL_openlibs(this->lua_state);
        main_mount();
        std::string filename = this->lua_dir + "/Init.lua";
        int isok = luaL_dofile(this->lua_state, filename.data());

        if (isok == LUA_OK)
        {
            LOG_ERROR("main Init.lua load succ");
        }
        else
        {
            LOG_ERROR("main Init.lua load failed, %s", lua_tostring(this->lua_state, -1));
            exit(-1);
        }
    }
    exe_OnMainInit(is_hot);
}

void lua_plugin::on_main_stop(bool is_hot)
{
    exe_OnMainStop(is_hot);
}

void lua_plugin::on_main_tick()
{
    if (this->lua_state_be_reload)
    {
        LOG_ERROR("this->lua_state_be_reload is true");
        this->lua_state_be_reload = false;

        on_main_stop(true);
        free_main_lua();
        if (this->lua_state != nullptr)
        {
            LOG_ERROR("free_main_lua after this->lua_state != nullptr");
            exit(-1);
        }
        real_on_main_init(true);
        return;
    }
    exe_OnMainTick();
}

void lua_plugin::on_worker_init(int worker_idx, bool is_hot)
{
    {
        this->worker_lua_state[worker_idx] = luaL_newstate();
        luaL_openlibs(this->worker_lua_state[worker_idx]);
        worker_mount(worker_idx);
        std::string filename = this->lua_dir + "/Init.lua";
        int isok = luaL_dofile(this->worker_lua_state[worker_idx], filename.data());
        if (isok == LUA_OK)
        {
            LOG_ERROR("worker vm[%d] Init.lua load succ", worker_idx);
        }
        else
        {
            LOG_ERROR("main worker vm[%d] Init.lua load failed, %s", worker_idx, lua_tostring(this->worker_lua_state[worker_idx], -1));
            exit(-1);
        }
    }
    exe_OnWorkerInit(worker_idx, is_hot);
}

void lua_plugin::on_worker_stop(int worker_idx, bool is_hot)
{
    exe_OnWorkerStop(worker_idx, is_hot);
}

void lua_plugin::on_worker_tick(int worker_idx)
{
    if (this->worker_lua_state_be_reload[worker_idx])
    {
        LOG_ERROR("this->worker_lua_state_be_reload[%d] is true", worker_idx);
        this->worker_lua_state_be_reload[worker_idx] = false;

        on_worker_stop(worker_idx, true);
        free_worker_lua(worker_idx);

        if (this->worker_lua_state[worker_idx] != nullptr)
        {
            LOG_ERROR("this->worker_lua_state[%d] != nullptr", worker_idx);
            exit(-1);
        }
        on_worker_init(worker_idx, true);
        return;
    }
    exe_OnWorkerTick(worker_idx);
}

void lua_plugin::exe_OnMainInit(bool is_hot)
{
    lua_getglobal(this->lua_state, "OnMainInit");
    lua_pushboolean(this->lua_state, is_hot);
    int isok = lua_pcall(this->lua_state, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnMainInit failed %s", lua_tostring(this->lua_state, -1));
    }
}

void lua_plugin::exe_OnMainStop(bool is_hot)
{
    lua_getglobal(this->lua_state, "OnMainStop");
    lua_pushboolean(this->lua_state, is_hot);
    int isok = lua_pcall(this->lua_state, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnMainStop failed %s", lua_tostring(this->lua_state, -1));
    }
}

void lua_plugin::exe_OnMainTick()
{
    lua_getglobal(this->lua_state, "OnMainTick");
    static int isok = 0;
    isok = lua_pcall(this->lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnMainTick failed %s", lua_tostring(this->lua_state, -1));
    }
}

void lua_plugin::exe_OnWorkerInit(int worker_idx, bool is_hot)
{
    lua_State *lua_ptr = this->worker_lua_state[worker_idx];
    lua_getglobal(lua_ptr, "OnWorkerInit");
    lua_pushinteger(lua_ptr, worker_idx);
    lua_pushboolean(lua_ptr, is_hot);
    int isok = lua_pcall(lua_ptr, 2, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnWorkerInit failed %s", lua_tostring(lua_ptr, -1));
    }
}

void lua_plugin::exe_OnWorkerStop(int worker_idx, bool is_hot)
{
    lua_State *lua_ptr = this->worker_lua_state[worker_idx];
    lua_getglobal(lua_ptr, "OnWorkerStop");
    lua_pushinteger(lua_ptr, worker_idx);
    lua_pushboolean(lua_ptr, is_hot);
    int isok = lua_pcall(lua_ptr, 2, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnWorkerStop failed %s", lua_tostring(lua_ptr, -1));
    }
}

void lua_plugin::exe_OnWorkerTick(int worker_idx)
{
    lua_State *lua_ptr = this->worker_lua_state[worker_idx];
    lua_getglobal(lua_ptr, "OnWorkerTick");
    lua_pushinteger(lua_ptr, worker_idx);
    int isok = lua_pcall(lua_ptr, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnWorkerTick failed %s", lua_tostring(lua_ptr, -1));
    }
}

void lua_plugin::exe_OnLuaVMRecvMessage(lua_State *lua_state, int cmd, const google::protobuf::Message &package)
{
    lua_plugin *lua_plugin_ptr = singleton<lua_plugin>::instance();

    lua_getglobal(lua_state, "OnLuaVMRecvMessage");

    int is_mainVM = 0;
    int is_otherVM = 0;
    int is_workerVM = 0;
    int worker_idx = -1;

    if (lua_plugin_ptr->lua_state == lua_state)
    {
        is_mainVM = 1;
    }
    else if (lua_plugin_ptr->other_lua_state == lua_state)
    {
        is_otherVM = 1;
    }
    else
    {
        for (int i = 0; i < lua_plugin_ptr->worker_lua_cnt; i++)
        {
            if (lua_state == lua_plugin_ptr->worker_lua_state[i])
            {
                worker_idx = i;
                is_workerVM = 1;
                break;
            }
        }
        if (worker_idx == -1)
        {
            LOG_ERROR("exe_OnLuaVMRecvMessage find lua_state worker_idx failed");
            return;
        }
    }

    lua_pushboolean(lua_state, is_mainVM);
    lua_pushboolean(lua_state, is_otherVM);
    lua_pushboolean(lua_state, is_workerVM);
    lua_pushinteger(lua_state, worker_idx);
    lua_pushinteger(lua_state, cmd);
    protobuf2lua(lua_state, package);

    int isok = lua_pcall(lua_state, 6, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnLuaVMRecvMessage failed %s", lua_tostring(lua_state, -1));
    }
}

void lua_plugin::on_other_init(bool is_hot)
{
    {
        this->other_lua_state = luaL_newstate();
        luaL_openlibs(this->other_lua_state);
        other_mount();
        std::string filename = this->lua_dir + "/Init.lua";
        int isok = luaL_dofile(this->other_lua_state, filename.data());

        if (isok == LUA_OK)
        {
            LOG_ERROR("other Init.lua load succ");
        }
        else
        {
            LOG_ERROR("other Init.lua load failed, %s", lua_tostring(this->other_lua_state, -1));
            exit(-1);
        }
    }
    exe_OnOtherInit(is_hot);
}

void lua_plugin::on_other_stop(bool is_hot)
{
    exe_OnOtherStop(is_hot);
}

void lua_plugin::on_other_tick()
{
    if (this->other_lua_state_be_reload)
    {
        LOG_ERROR("this->other_lua_state_be_reload is true");
        this->other_lua_state_be_reload = false;

        on_other_stop(true);
        free_other_lua();

        if (this->other_lua_state != nullptr)
        {
            LOG_ERROR("this->other_lua_state != nullptr");
            exit(-1);
        }

        on_other_init(true);

        return;
    }
    exe_OnOtherTick();
}

void lua_plugin::exe_OnOtherInit(bool is_hot)
{
    lua_getglobal(this->other_lua_state, "OnOtherInit");
    static int isok = 0;
    lua_pushboolean(this->other_lua_state, is_hot);
    isok = lua_pcall(this->other_lua_state, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnOtherInit failed %s", lua_tostring(this->other_lua_state, -1));
    }
}

void lua_plugin::exe_OnOtherStop(bool is_hot)
{
    lua_getglobal(this->other_lua_state, "OnOtherStop");
    static int isok = 0;
    lua_pushboolean(this->other_lua_state, is_hot);
    isok = lua_pcall(this->other_lua_state, 1, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnOtherStop failed %s", lua_tostring(this->other_lua_state, -1));
    }
}

void lua_plugin::exe_OnOtherTick()
{
    lua_getglobal(this->other_lua_state, "OnOtherTick");
    static int isok = 0;
    isok = lua_pcall(this->other_lua_state, 0, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnOtherTick failed %s", lua_tostring(this->other_lua_state, -1));
    }
}

void lua_plugin::main_mount()
{
    static luaL_Reg main_lulibs[] = {
        {"Logger", Logger},
        {"Lua2Protobuf", Lua2Protobuf},
        {NULL, NULL}};
    {
        // mount main lua vm
        luaL_newlib(this->lua_state, main_lulibs);
        lua_setglobal(this->lua_state, "avant");
    }
}

void lua_plugin::worker_mount(int worker_idx)
{
    static luaL_Reg worker_lulibs[] = {
        {"Logger", Logger},
        {"Lua2Protobuf", Lua2Protobuf},
        {NULL, NULL}};
    luaL_newlib(this->worker_lua_state[worker_idx], worker_lulibs);
    lua_setglobal(this->worker_lua_state[worker_idx], "avant");
}

void lua_plugin::other_mount()
{
    static luaL_Reg other_lulibs[] = {
        {"Logger", Logger},
        {"Lua2Protobuf", Lua2Protobuf},
        {NULL, NULL}};
    {
        luaL_newlib(this->other_lua_state, other_lulibs);
        lua_setglobal(this->other_lua_state, "avant");
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

int lua_plugin::Lua2Protobuf(lua_State *lua_state)
{
    int num = lua_gettop(lua_state);
    if (num != 2)
    {
        LOG_ERROR("num!=2");
        lua_pushnil(lua_state);
        return 1;
    }
    if (lua_isinteger(lua_state, 2) == 0)
    {
        LOG_ERROR("lua_isinteger 2 false");
        lua_pushnil(lua_state);
        return 1;
    }
    int cmd = lua_tointeger(lua_state, 2);
    lua_pop(lua_state, 1);

    if (lua_istable(lua_state, 1) == 0)
    {
        LOG_ERROR("lua_istable 1 false");
        lua_pushnil(lua_state);
        return 1;
    }

    if (cmd == ProtoCmd::PROTO_CMD_LUA_TEST)
    {
        ProtoLuaTest proto_lua_test;
        lua2protobuf(lua_state, proto_lua_test);
        LOG_ERROR("ProtoLuaTest %s", proto_lua_test.DebugString().c_str());
        // call lua function OnLuaVMRecvMessage
        exe_OnLuaVMRecvMessage(lua_state, cmd, proto_lua_test);
        lua_pushinteger(lua_state, 0);
        return 1;
    }

    LOG_ERROR("cmd[%d] != ProtoCmd::PROTO_CMD_LUA_TEST", cmd);
    lua_pushnil(lua_state);
    return 1;
}

void lua_plugin::lua2protobuf(lua_State *L, google::protobuf::Message &package)
{
    const google::protobuf::Descriptor *descripter = package.GetDescriptor();
    const google::protobuf::Reflection *reflection = package.GetReflection();

    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        const char *key = lua_tostring(L, -2);
        const google::protobuf::FieldDescriptor *field = descripter->FindFieldByName(key);
        if (!field)
        {
            lua_pop(L, 1);
            continue;
        }
        switch (field->cpp_type())
        {
        case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddInt32(&package, field, lua_tointeger(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetInt32(&package, field, lua_tointeger(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddInt64(&package, field, lua_tointeger(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetInt64(&package, field, lua_tointeger(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddUInt32(&package, field, lua_tointeger(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetUInt32(&package, field, lua_tointeger(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddUInt64(&package, field, lua_tointeger(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetUInt64(&package, field, lua_tointeger(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddDouble(&package, field, lua_tonumber(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetDouble(&package, field, lua_tonumber(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddDouble(&package, field, lua_tonumber(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetDouble(&package, field, lua_tonumber(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddBool(&package, field, lua_toboolean(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetBool(&package, field, lua_toboolean(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    reflection->AddString(&package, field, lua_tostring(L, -1));
                    lua_pop(L, 1);
                }
            }
            else
            {
                reflection->SetString(&package, field, lua_tostring(L, -1));
            }
            break;
        }
        case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
        {
            if (field->is_repeated())
            {
                int n = lua_rawlen(L, -1);
                for (int i = 1; i <= n; ++i)
                {
                    lua_rawgeti(L, -1, i);
                    google::protobuf::Message *subMessage = reflection->AddMessage(&package, field);
                    lua2protobuf(L, *subMessage);
                    lua_pop(L, 1);
                }
            }
            else
            {
                google::protobuf::Message *subMessage = reflection->MutableMessage(&package, field);
                lua2protobuf(L, *subMessage);
            }
            break;
        }
        default:
        {
            break;
        }
        }
        lua_pop(L, 1);
    }
}

void lua_plugin::protobuf2lua(lua_State *L, const google::protobuf::Message &package)
{
    const google::protobuf::Descriptor *descriptor = package.GetDescriptor();
    const google::protobuf::Reflection *reflection = package.GetReflection();

    lua_newtable(L);

    for (int i = 0; i < descriptor->field_count(); i++)
    {
        const google::protobuf::FieldDescriptor *field = descriptor->field(i);

        lua_pushstring(L, field->name().c_str());
        // LOG_ERROR("%s %s", field->name().c_str(), field->cpp_type_name());

        if (field->is_repeated())
        {
            lua_newtable(L);
            int field_size = reflection->FieldSize(package, field);
            for (int j = 0; j < field_size; ++j)
            {
                lua_pushinteger(L, j + 1);
                int cpp_type = field->cpp_type();
                switch (cpp_type)
                {
                case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
                {
                    lua_pushinteger(L, reflection->GetRepeatedInt32(package, field, j));
                    // LOG_ERROR("%d", reflection->GetRepeatedInt32(package, field, j));
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
                {
                    lua_pushinteger(L, reflection->GetRepeatedInt64(package, field, j));
                    // LOG_ERROR("%lld", reflection->GetRepeatedInt64(package, field, j));
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
                {
                    lua_pushinteger(L, reflection->GetRepeatedUInt32(package, field, j));
                    // LOG_ERROR("%u", reflection->GetRepeatedUInt32(package, field, j));
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
                {
                    lua_pushinteger(L, reflection->GetRepeatedUInt64(package, field, j));
                    // LOG_ERROR("%llu", reflection->GetRepeatedUInt64(package, field, j));
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
                {
                    lua_pushnumber(L, reflection->GetRepeatedDouble(package, field, j));
                    // LOG_ERROR("%lf", reflection->GetRepeatedDouble(package, field, j));
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
                {
                    lua_pushnumber(L, reflection->GetRepeatedFloat(package, field, j));
                    // LOG_ERROR("%f", reflection->GetRepeatedFloat(package, field, j));
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
                {
                    int bool2int = reflection->GetRepeatedBool(package, field, j);
                    lua_pushboolean(L, bool2int);
                    // LOG_ERROR("%d", bool2int);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
                {
                    lua_pushlstring(L, reflection->GetRepeatedString(package, field, j).c_str(), reflection->GetRepeatedString(package, field, j).size());
                    // LOG_ERROR("%s", reflection->GetRepeatedString(package, field, j).c_str());
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
                {
                    protobuf2lua(L, reflection->GetRepeatedMessage(package, field, j));
                    break;
                }
                default:
                {
                    lua_pushnil(L);
                    break;
                }
                }
                lua_settable(L, -3);
            }
        }
        else
        {
            int cpp_type = field->cpp_type();
            switch (cpp_type)
            {
            case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
            {
                lua_pushinteger(L, reflection->GetInt32(package, field));
                // LOG_ERROR("%d", reflection->GetInt32(package, field));
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
            {
                lua_pushinteger(L, reflection->GetInt64(package, field));
                // LOG_ERROR("%lld", reflection->GetInt64(package, field));
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
            {
                lua_pushinteger(L, reflection->GetUInt32(package, field));
                // LOG_ERROR("%u", reflection->GetUInt32(package, field));
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
            {
                lua_pushinteger(L, reflection->GetUInt64(package, field));
                // LOG_ERROR("%llu", reflection->GetUInt64(package, field));
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
            {
                lua_pushnumber(L, reflection->GetDouble(package, field));
                // LOG_ERROR("%lf", reflection->GetDouble(package, field));
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
            {
                lua_pushnumber(L, reflection->GetFloat(package, field));
                // LOG_ERROR("%f", reflection->GetFloat(package, field));
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
            {
                int bool2int = reflection->GetBool(package, field);
                lua_pushboolean(L, bool2int);
                // LOG_ERROR("%d", bool2int);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
            {
                lua_pushlstring(L, reflection->GetString(package, field).c_str(), reflection->GetString(package, field).size());
                // LOG_ERROR("%s", reflection->GetString(package, field).c_str());
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
            {
                protobuf2lua(L, reflection->GetMessage(package, field));
                break;
            }
            default:
            {
                lua_pushnil(L);
                break;
            }
            }
        }
        lua_settable(L, -3);
    }
}