#include "app/lua_plugin.h"
#include <avant-log/logger.h>
#include <string>
#include <cstdlib>
#include "proto/proto_util.h"
#include "proto_res/proto_lua.pb.h"
#include "utility/singleton.h"
#include <stack>

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

    real_on_main_init();
}

void lua_plugin::real_on_main_init()
{
    // init main lua vm
    {
        this->lua_state = luaL_newstate();
        luaL_openlibs(this->lua_state);
        main_mount();
        std::string filename = this->lua_dir + "/Init.lua";
        int isok = luaL_dofile(this->lua_state, filename.data());

        ASSERT_LOG_EXIT(isok == LUA_OK);
    }

    int old_lua_stack_size = lua_gettop(this->lua_state);
    exe_OnMainInit();
    int new_lua_stack_size = lua_gettop(this->lua_state);
    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::on_main_stop()
{
    int old_lua_stack_size = lua_gettop(this->lua_state);
    exe_OnMainStop();
    int new_lua_stack_size = lua_gettop(this->lua_state);

    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::on_main_tick()
{
    if (this->lua_state_be_reload)
    {
        LOG_ERROR("this->lua_state_be_reload is true");
        this->lua_state_be_reload = false;

        int old_lua_stack_size = lua_gettop(this->lua_state);
        exe_OnMainReload();
        int new_lua_stack_size = lua_gettop(this->lua_state);

        ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);

        return;
    }

    int old_lua_stack_size = lua_gettop(this->lua_state);
    exe_OnMainTick();
    int new_lua_stack_size = lua_gettop(this->lua_state);
    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::on_worker_init(int worker_idx)
{
    {
        this->worker_lua_state[worker_idx] = luaL_newstate();
        luaL_openlibs(this->worker_lua_state[worker_idx]);
        worker_mount(worker_idx);
        std::string filename = this->lua_dir + "/Init.lua";
        int isok = luaL_dofile(this->worker_lua_state[worker_idx], filename.data());

        ASSERT_LOG_EXIT(isok == LUA_OK);
    }

    int old_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
    exe_OnWorkerInit(worker_idx);
    int new_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::on_worker_stop(int worker_idx)
{
    int old_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
    exe_OnWorkerStop(worker_idx);
    int new_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::on_worker_tick(int worker_idx)
{
    if (this->worker_lua_state_be_reload[worker_idx])
    {
        LOG_ERROR("this->worker_lua_state_be_reload[%d] is true", worker_idx);
        this->worker_lua_state_be_reload[worker_idx] = false;

        int old_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
        exe_OnWorkerReload(worker_idx);
        int new_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
        ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);

        return;
    }

    int old_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
    exe_OnWorkerTick(worker_idx);
    int new_lua_stack_size = lua_gettop(this->worker_lua_state[worker_idx]);
    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::exe_OnMainInit()
{
    int isok = LUA_OK;
    lua_getglobal(this->lua_state, "OnMainInit");

    int old_lua_stack_size = lua_gettop(this->lua_state);
    isok = lua_pcall(this->lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(isok == LUA_OK);
}

void lua_plugin::exe_OnMainStop()
{
    int isok = LUA_OK;
    lua_getglobal(this->lua_state, "OnMainStop");

    isok = lua_pcall(this->lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(LUA_OK == isok);
}

void lua_plugin::exe_OnMainTick()
{
    static int isok = LUA_OK;
    lua_getglobal(this->lua_state, "OnMainTick");

    isok = lua_pcall(this->lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(LUA_OK == isok);
}

void lua_plugin::exe_OnMainReload()
{
    static int isok = LUA_OK;

    lua_getglobal(this->lua_state, "OnMainReload");

    isok = lua_pcall(this->lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(LUA_OK == isok);
}

void lua_plugin::exe_OnWorkerInit(int worker_idx)
{
    lua_State *lua_ptr = this->worker_lua_state[worker_idx];

    int isok = LUA_OK;
    lua_getglobal(lua_ptr, "OnWorkerInit");

    lua_pushinteger(lua_ptr, worker_idx);

    isok = lua_pcall(lua_ptr, 1, 0, 0);
    ASSERT_LOG_EXIT(LUA_OK == isok);
}

void lua_plugin::exe_OnWorkerStop(int worker_idx)
{
    int isok = LUA_OK;

    lua_State *lua_ptr = this->worker_lua_state[worker_idx];

    lua_getglobal(lua_ptr, "OnWorkerStop");

    lua_pushinteger(lua_ptr, worker_idx);
    isok = lua_pcall(lua_ptr, 1, 0, 0);
    ASSERT_LOG_EXIT(LUA_OK == isok);
}

void lua_plugin::exe_OnWorkerTick(int worker_idx)
{
    lua_State *lua_ptr = this->worker_lua_state[worker_idx];

    int isok = LUA_OK;
    lua_getglobal(lua_ptr, "OnWorkerTick");

    lua_pushinteger(lua_ptr, worker_idx);
    isok = lua_pcall(lua_ptr, 1, 0, 0);
    ASSERT_LOG_EXIT(LUA_OK == isok);
}

void lua_plugin::exe_OnWorkerReload(int worker_idx)
{
    lua_State *lua_ptr = this->worker_lua_state[worker_idx];

    int isok = LUA_OK;
    lua_getglobal(lua_ptr, "OnWorkerReload");

    lua_pushinteger(lua_ptr, worker_idx);
    isok = lua_pcall(lua_ptr, 1, 0, 0);
    ASSERT_LOG_EXIT(LUA_OK == isok);
}

void lua_plugin::exe_OnLuaVMRecvMessage(lua_State *lua_state, int cmd, const google::protobuf::Message &package)
{
    lua_plugin *lua_plugin_ptr = singleton<lua_plugin>::instance();

    int isok = LUA_OK;
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
        ASSERT_LOG_EXIT(worker_idx != -1);
    }

    lua_pushboolean(lua_state, is_mainVM);
    lua_pushboolean(lua_state, is_otherVM);
    lua_pushboolean(lua_state, is_workerVM);
    lua_pushinteger(lua_state, worker_idx);
    lua_pushinteger(lua_state, cmd);
    protobuf2lua(lua_state, package);

    isok = lua_pcall(lua_state, 6, 0, 0);
    ASSERT_LOG_EXIT(isok == LUA_OK);
}

void lua_plugin::on_other_init()
{
    {
        this->other_lua_state = luaL_newstate();
        luaL_openlibs(this->other_lua_state);
        other_mount();
        std::string filename = this->lua_dir + "/Init.lua";
        int isok = luaL_dofile(this->other_lua_state, filename.data());

        ASSERT_LOG_EXIT(isok == LUA_OK);
    }

    int old_lua_stack_size = lua_gettop(this->other_lua_state);
    exe_OnOtherInit();
    int new_lua_stack_size = lua_gettop(this->other_lua_state);

    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::on_other_stop()
{
    int old_lua_stack_size = lua_gettop(this->other_lua_state);
    exe_OnOtherStop();
    int new_lua_stack_size = lua_gettop(this->other_lua_state);
    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::on_other_tick()
{
    if (this->other_lua_state_be_reload)
    {
        LOG_ERROR("this->other_lua_state_be_reload is true");
        this->other_lua_state_be_reload = false;

        int old_lua_stack_size = lua_gettop(this->other_lua_state);
        exe_OnOtherReload();
        int new_lua_stack_size = lua_gettop(this->other_lua_state);
        ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);

        return;
    }

    int old_lua_stack_size = lua_gettop(this->other_lua_state);
    exe_OnOtherTick();
    int new_lua_stack_size = lua_gettop(this->other_lua_state);
    ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);
}

void lua_plugin::exe_OnOtherInit()
{
    static int isok = LUA_OK;

    lua_getglobal(this->other_lua_state, "OnOtherInit");

    isok = lua_pcall(this->other_lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(isok == LUA_OK);
}

void lua_plugin::exe_OnOtherStop()
{
    static int isok = LUA_OK;
    lua_getglobal(this->other_lua_state, "OnOtherStop");

    isok = lua_pcall(this->other_lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(isok == LUA_OK);
}

void lua_plugin::exe_OnOtherTick()
{
    static int isok = LUA_OK;
    lua_getglobal(this->other_lua_state, "OnOtherTick");

    isok = lua_pcall(this->other_lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(isok == LUA_OK);
}

void lua_plugin::exe_OnOtherReload()
{
    static int isok = LUA_OK;
    lua_getglobal(this->other_lua_state, "OnOtherReload");

    isok = lua_pcall(this->other_lua_state, 0, 0, 0);
    ASSERT_LOG_EXIT(isok == LUA_OK);
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

        lua_pushstring(this->lua_state, this->lua_dir.c_str());
        lua_setfield(this->lua_state, -2, "LuaDir");

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

    lua_pushstring(this->worker_lua_state[worker_idx], this->lua_dir.c_str());
    lua_setfield(this->worker_lua_state[worker_idx], -2, "LuaDir");

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

        lua_pushstring(this->other_lua_state, this->lua_dir.c_str());
        lua_setfield(this->other_lua_state, -2, "LuaDir");

        lua_setglobal(this->other_lua_state, "avant");
    }
}

int lua_plugin::Logger(lua_State *lua_state)
{
    int num = lua_gettop(lua_state);
    ASSERT_LOG_EXIT(num == 1);

    int isok = LUA_OK;
    isok = lua_isstring(lua_state, 1);
    ASSERT_LOG_EXIT(isok);

    size_t len = 0;
    const char *type = lua_tolstring(lua_state, 1, &len);
    std::string str(type, len);
    LOG_ERROR("lua => %s", str.data());
    lua_pop(lua_state, 1);
    lua_pushinteger(lua_state, 0);
    return 1;
}

// 此处只是测试 lua其实不应该直接调用 lua_plugin::Lua2Protobuf
// 而是有C++调用进行解析 此处还在开发阶段
int lua_plugin::Lua2Protobuf(lua_State *lua_state)
{
    int num = lua_gettop(lua_state);
    ASSERT_LOG_EXIT(num == 2);

    int isok = lua_isinteger(lua_state, 2);
    ASSERT_LOG_EXIT(isok);

    int cmd = lua_tointeger(lua_state, 2);
    lua_pop(lua_state, 1); // 弹出cmd

    isok = lua_istable(lua_state, 1);
    ASSERT_LOG_EXIT(isok);

    int old_lua_stack_size = lua_gettop(lua_state);

    // 注意应该把消息通过管道传输而不是直接调用exe_onLuaVMRecvMessage不然可能造成递归
    if (cmd == ProtoCmd::PROTO_CMD_LUA_TEST)
    {
        ProtoLuaTest proto_lua_test;
        // TODO FIX 栈不平衡
        lua2protobuf_nostack(lua_state, proto_lua_test);

        // LOG_ERROR("ProtoLuaTest\n%s", proto_lua_test.DebugString().c_str());

        exe_OnLuaVMRecvMessage(lua_state, cmd, proto_lua_test);
        int new_lua_stack_size = lua_gettop(lua_state);

        ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);

        lua_pop(lua_state, 1); // 弹出val
        lua_pushinteger(lua_state, 0);
        return 1;
    }

    lua_pop(lua_state, 1); // 弹出val
    LOG_ERROR("cmd[%d] != ProtoCmd::PROTO_CMD_LUA_TEST", cmd);
    lua_pushnil(lua_state);
    return 1;
}

// lua2protobuf非递归版 递归版已舍弃
void lua_plugin::lua2protobuf_nostack(lua_State *L, const google::protobuf::Message &package)
{
    struct StackFrame
    {
        google::protobuf::Message *package_ptr{nullptr};
        uint32_t frame_in_luastack{0};
        std::shared_ptr<std::string> curr_key_str_ptr{nullptr};
        bool curr_key_is_nil{false};
        char luapop_num_on_destory{0};
    };
    std::stack<StackFrame> cpp_stack;

    // 初始栈帧
    {
        StackFrame frame;
        frame.package_ptr = const_cast<google::protobuf::Message *>(&package);
        frame.frame_in_luastack = 1;
        frame.curr_key_is_nil = true;
        frame.luapop_num_on_destory = 0;
        cpp_stack.push(frame);
        // LOG_DEBUG("创建栈帧 %s", frame.package_ptr->GetDescriptor()->name().c_str());
    }

    // C++栈不为空时
    while (!cpp_stack.empty())
    {
        StackFrame &frame = cpp_stack.top();

        const google::protobuf::Descriptor *descripter = frame.package_ptr->GetDescriptor();
        const google::protobuf::Reflection *reflection = frame.package_ptr->GetReflection();

        if (frame.curr_key_is_nil)
        {
            frame.curr_key_is_nil = false;
            // LOG_DEBUG("首次处理 %s 内部字段", frame.package_ptr->GetDescriptor()->name().c_str());
            lua_pushnil(L);
        }
        else
        {
            // LOG_DEBUG("已处理过 %s:%s", frame.package_ptr->GetDescriptor()->name().c_str(), frame.curr_key_str_ptr->c_str());
            lua_pushstring(L, frame.curr_key_str_ptr->c_str());
        }

        // LOG_DEBUG("%s frame_in_luastack %d Lua栈大小 %d", frame.package_ptr->GetDescriptor()->name().c_str(), frame.frame_in_luastack, lua_gettop(L));

        if (lua_next(L, frame.frame_in_luastack) != 0)
        {
            // LOG_DEBUG("进入 if lua_next Lua栈大小 %d", lua_gettop(L));
            const char *next_key = lua_tostring(L, -2);
            // LOG_DEBUG("next_key %s", next_key);
            frame.curr_key_str_ptr = std::make_shared<std::string>(next_key);

            const google::protobuf::FieldDescriptor *field = descripter->FindFieldByName(frame.curr_key_str_ptr->c_str());
            if (!field)
            {
                LOG_ERROR("lua2protobuf field[%s] not found in package[%s]", frame.curr_key_str_ptr->c_str(), descripter->name().c_str());
                // 把栈顶刚才解析出的key,val弹出
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                continue;
            }

            switch (field->cpp_type())
            {
            case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
            {
                // 这个key对应的是数组
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        // 获取-1位置的val中的第i个元素后放入栈顶
                        lua_rawgeti(L, -1, arr_idx);
                        // 获取栈顶的值
                        // LOG_DEBUG("%d", lua_tointeger(L, -1));
                        reflection->AddInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                        // 弹出栈顶的值
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%d", lua_tointeger(L, -1));
                    reflection->SetInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                }
                // 把栈顶刚才解析出的val弹出
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
            {
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        // LOG_DEBUG("%d", lua_tointeger(L, -1));
                        reflection->AddInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%d", lua_tointeger(L, -1));
                    reflection->SetInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                }
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
            {
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        // LOG_DEBUG("%d", lua_tointeger(L, -1));
                        reflection->AddUInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%d", lua_tointeger(L, -1));
                    reflection->SetUInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                }
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
            {
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        // LOG_DEBUG("%d", lua_tointeger(L, -1));
                        reflection->AddUInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%d", lua_tointeger(L, -1));
                    reflection->SetUInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                }
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
            {
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        // LOG_DEBUG("%lf", lua_tonumber(L, -1));
                        reflection->AddDouble(frame.package_ptr, field, lua_tonumber(L, -1));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%lf", lua_tonumber(L, -1));
                    reflection->SetDouble(frame.package_ptr, field, lua_tonumber(L, -1));
                }
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
            {
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        // LOG_DEBUG("%lf", lua_tonumber(L, -1));
                        reflection->AddFloat(frame.package_ptr, field, lua_tonumber(L, -1));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%lf", lua_tonumber(L, -1));
                    reflection->SetFloat(frame.package_ptr, field, lua_tonumber(L, -1));
                }
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
            {
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        // LOG_DEBUG("%d", lua_toboolean(L, -1));
                        reflection->AddBool(frame.package_ptr, field, lua_toboolean(L, -1));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%d", lua_toboolean(L, -1));
                    reflection->SetBool(frame.package_ptr, field, lua_toboolean(L, -1));
                }
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
            {
                if (field->is_repeated())
                {
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        // LOG_DEBUG("%s", lua_tostring(L, -1));
                        reflection->AddString(frame.package_ptr, field, lua_tostring(L, -1));
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    // LOG_DEBUG("%s", lua_tostring(L, -1));
                    reflection->SetString(frame.package_ptr, field, lua_tostring(L, -1));
                }
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
            {
                // 到这里lua栈顶目前是本次field的 val与key
                // 要创建新的栈帧了
                if (field->is_repeated())
                {
                    // arr_item_val
                    // arr_item_val
                    // field_val 等arr_item_val栈帧处理完后需要清除
                    // field_key 等arr_item_val栈帧处理完后需要清除
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1 - (arr_idx - 1), arr_idx); // 会将arr_idx处读的值放到栈顶
                        google::protobuf::Message *subMessage = reflection->AddMessage(frame.package_ptr, field);

                        // LOG_DEBUG("AddMessage subMessage %s", subMessage->GetDescriptor()->name().c_str());

                        int n_in_luastack = lua_gettop(L); // lua栈中现在有几个元素
                        StackFrame new_frame;
                        new_frame.package_ptr = subMessage;
                        new_frame.frame_in_luastack = n_in_luastack;
                        new_frame.curr_key_str_ptr = nullptr;
                        new_frame.curr_key_is_nil = true;
                        new_frame.luapop_num_on_destory = (arr_idx == 1) ? 3 : 1;
                        cpp_stack.push(new_frame);
                        // LOG_DEBUG("创建栈帧 %s", new_frame.package_ptr->GetDescriptor()->name().c_str());
                    }
                }
                else
                {
                    // field_val 等arr_item_val栈帧处理完后需要清除
                    // field_key 等arr_item_val栈帧处理完后需要清除
                    google::protobuf::Message *subMessage = reflection->MutableMessage(frame.package_ptr, field);
                    int n_in_luastack = lua_gettop(L); // lua栈中现在有几个元素
                    StackFrame new_frame;
                    new_frame.package_ptr = subMessage;
                    new_frame.frame_in_luastack = n_in_luastack;
                    new_frame.curr_key_str_ptr = nullptr;
                    new_frame.curr_key_is_nil = true;
                    new_frame.luapop_num_on_destory = 2;
                    cpp_stack.push(new_frame);
                    // LOG_DEBUG("创建栈帧 %s", new_frame.package_ptr->GetDescriptor()->name().c_str());
                }

                break;
            }
            default:
            {
                lua_pop(L, 1); // field_val
                lua_pop(L, 1); // field_key
                LOG_ERROR("field->cpp_type() %d key[%s] not support in [%s]", field->cpp_type(), frame.curr_key_str_ptr->c_str(), descripter->name().c_str());
                break;
            }
            }
        }
        else // 这帧彻底处理完毕了
        {
            // 函数传过来的创建的首帧
            if (frame.package_ptr == &package)
            {
                // LOG_DEBUG("弹出首帧栈帧 field %s", frame.package_ptr->GetDescriptor()->name().c_str());
                cpp_stack.pop();
                // 保留函数调用过lua栈传过来的首个val
            }
            else // while过程中产生的帧
            {
                // LOG_DEBUG("弹出中间栈帧 field %s luapop_num_on_destory %d", frame.package_ptr->GetDescriptor()->name().c_str(), (int)frame.luapop_num_on_destory);

                if (frame.luapop_num_on_destory == 1)
                {
                    lua_pop(L, 1); // arr_item_val
                }
                else if (frame.luapop_num_on_destory == 2)
                {
                    lua_pop(L, 1); // field_val
                    lua_pop(L, 1); // field_key
                }
                else if (frame.luapop_num_on_destory == 3)
                {
                    lua_pop(L, 1); // arr_item_val
                    lua_pop(L, 1); // field_val
                    lua_pop(L, 1); // field_key
                }

                cpp_stack.pop();
            }
        }
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