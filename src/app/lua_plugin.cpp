#include "app/lua_plugin.h"
#include <avant-log/logger.h>
#include <string>
#include <cstdlib>
#include "proto/proto_util.h"
#include "proto_res/proto_lua.pb.h"
#include "utility/singleton.h"

using namespace avant::app;
using namespace avant::utility;

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

void lua_plugin::exe_OnWorkerRecvMessage(lua_State *lua_state, int cmd, const google::protobuf::Message &package)
{
    lua_plugin *lua_plugin_ptr = singleton<lua_plugin>::instance();

    lua_getglobal(lua_state, "OnWorkerRecvMessage");

    int worker_idx = -1;
    for (int i = 0; i < lua_plugin_ptr->worker_lua_cnt; i++)
    {
        if (lua_state == lua_plugin_ptr->worker_lua_state[i])
        {
            worker_idx = i;
            break;
        }
    }
    if (worker_idx == -1)
    {
        LOG_ERROR("exe_OnWorkerRecvMessage find lua_state worker_idx failed");
        return;
    }

    lua_pushinteger(lua_state, worker_idx);
    lua_pushinteger(lua_state, cmd);
    protobuf2lua(lua_state, package);

    int isok = lua_pcall(lua_state, 3, 0, 0);
    if (LUA_OK != isok)
    {
        LOG_ERROR("exe_OnWorkerRecvMessage failed %s", lua_tostring(lua_state, -1));
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
        {"Lua2Protobuf", Lua2Protobuf},
        {NULL, NULL}};
    {
        // mount main lua vm
        luaL_newlib(lua_state, main_lulibs);
        lua_setglobal(lua_state, "avant");
    }

    static luaL_Reg worker_lulibs[] = {
        {"Logger", Logger},
        {"Lua2Protobuf", Lua2Protobuf},
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
        {"Lua2Protobuf", Lua2Protobuf},
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
    if (cmd != ProtoCmd::PROTO_CMD_LUA_TEST)
    {
        LOG_ERROR("cmd[%d] != ProtoCmd::PROTO_CMD_LUA_TEST", cmd);
        lua_pushnil(lua_state);
        return 1;
    }

    ProtoLuaTest proto_lua_test;
    lua2protobuf(lua_state, proto_lua_test);
    LOG_ERROR("ProtoLuaTest %s", proto_lua_test.DebugString().c_str());

    // call lua function OnWorkerRecvMessage
    exe_OnWorkerRecvMessage(lua_state, cmd, proto_lua_test);

    lua_pushinteger(lua_state, 0);
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