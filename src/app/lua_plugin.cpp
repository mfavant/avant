#include "app/lua_plugin.h"
#include <avant-log/logger.h>
#include <string>
#include <cstdlib>
#include "proto/proto_util.h"
#include "proto_res/proto_lua.pb.h"
#include "utility/singleton.h"
#include <stack>
#include <chrono>

using namespace avant::app;
using namespace avant::utility;

// 屏蔽所有调试日志
#define LOG_LUA_PLUGIN_RUNTIME(...) ((void)0)
// #define LOG_LUA_PLUGIN_RUNTIME(...) LOG_DEBUG(__VA_ARGS__)

lua_plugin::lua_plugin()
{
    init_message_factory();
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

    int old_lua_stack_size = lua_gettop(lua_state);
    protobuf2lua_nostack(lua_state, package);
    int new_lua_stack_size = lua_gettop(lua_state);
    ASSERT_LOG_EXIT(new_lua_stack_size == old_lua_stack_size + 1);

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
        {"HighresTime", HighresTime},
        {"Monotonic", Monotonic},
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
        {"HighresTime", HighresTime},
        {"Monotonic", Monotonic},
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
        {"HighresTime", HighresTime},
        {"Monotonic", Monotonic},
        {NULL, NULL}};
    {
        luaL_newlib(this->other_lua_state, other_lulibs);

        lua_pushstring(this->other_lua_state, this->lua_dir.c_str());
        lua_setfield(this->other_lua_state, -2, "LuaDir");

        lua_setglobal(this->other_lua_state, "avant");
    }
}

// 返回两个值：1) seconds (double), 2) nanoseconds (integer)
int lua_plugin::HighresTime(lua_State *lua_state)
{
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ns = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    double seconds = static_cast<double>(ns) / 1e9;
    lua_pushnumber(lua_state, seconds);                       // pushes double seconds
    lua_pushinteger(lua_state, static_cast<lua_Integer>(ns)); // pushes integer nanoseconds
    return 2;
}

// 返回单个值：monotonic seconds (integer)
int lua_plugin::Monotonic(lua_State *lua_state)
{
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto ns = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    lua_pushinteger(lua_state, static_cast<lua_Integer>(ns)); // pushes integer nanoseconds
    return 1;
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
    std::shared_ptr<google::protobuf::Message> msg_ptr = singleton<lua_plugin>::instance()->protobuf_cmd2message(cmd);
    if (msg_ptr)
    {
        // lua栈必须平衡
        lua2protobuf_nostack(lua_state, *msg_ptr);
        int new_lua_stack_size = lua_gettop(lua_state);
        ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);

        LOG_LUA_PLUGIN_RUNTIME("ProtoLuaTest\n%s", msg_ptr->DebugString().c_str());

        // 模拟向lua发包
        exe_OnLuaVMRecvMessage(lua_state, cmd, *msg_ptr);

        new_lua_stack_size = lua_gettop(lua_state);
        ASSERT_LOG_EXIT(old_lua_stack_size == new_lua_stack_size);

        lua_pop(lua_state, 1); // 弹出val
        lua_pushinteger(lua_state, 0);
        return 1;
    }
    else
    {
        LOG_ERROR("protobuf_cmd2message(%d) return nullptr", cmd);
    }

    lua_pop(lua_state, 1); // 弹出val
    LOG_FATAL("cmd[%d] not be handle", cmd);
    lua_pushnil(lua_state);
    return 1;
}

// lua2protobuf非递归版 递归版已舍弃
void lua_plugin::lua2protobuf_nostack(lua_State *L, const google::protobuf::Message &package)
{
    if (!lua_istable(L, -1))
    {
        LOG_FATAL("lua2protobuf_nostack 只能处理lua table");
        return;
    }

    struct StackFrame
    {
        google::protobuf::Message *package_ptr{nullptr};
        uint32_t frame_in_luastack{0};
        bool curr_key_is_nil{false};
        signed char luapop_num_on_destory{0};
    };
    std::stack<StackFrame> cpp_stack;

    // 初始栈帧
    {
        StackFrame new_frame;
        new_frame.package_ptr = const_cast<google::protobuf::Message *>(&package);
        new_frame.frame_in_luastack = 1;
        new_frame.curr_key_is_nil = true;
        new_frame.luapop_num_on_destory = 0;
        cpp_stack.push(new_frame);
        LOG_LUA_PLUGIN_RUNTIME("创建首帧 %s", new_frame.package_ptr->GetDescriptor()->name().c_str());
    }

    // C++栈不为空时
    while (!cpp_stack.empty())
    {
        StackFrame &frame = cpp_stack.top();

        const google::protobuf::Descriptor *descriptor = frame.package_ptr->GetDescriptor();
        const google::protobuf::Reflection *reflection = frame.package_ptr->GetReflection();

        // 迭代frame.package_ptr下的所有字段 frame.curr_key_is_nil为真则从头开始迭代
        if (frame.curr_key_is_nil)
        {
            frame.curr_key_is_nil = false;
            LOG_LUA_PLUGIN_RUNTIME("首次开始遍历 %s 内部字段", frame.package_ptr->GetDescriptor()->name().c_str());
            lua_pushnil(L);
        }
        else
        {
            LOG_LUA_PLUGIN_RUNTIME("继续遍历 %s 内部字段", frame.package_ptr->GetDescriptor()->name().c_str());
        }

        LOG_LUA_PLUGIN_RUNTIME("%s frame_in_luastack %d Lua栈大小 %d", frame.package_ptr->GetDescriptor()->name().c_str(), frame.frame_in_luastack, lua_gettop(L));

        if (lua_next(L, frame.frame_in_luastack) != 0)
        {
            // 判断键的类型
            LOG_LUA_PLUGIN_RUNTIME("在 %s 中发现一个类型为 %s 的键", frame.package_ptr->GetDescriptor()->name().c_str(), luaL_typename(L, -2));
            // lua2protobuf_nostack只支持处理 integer为键或string为键其他一律不支持没有意义

            std::string this_loop_key;

            if (lua_isinteger(L, -2)) // 优先判断是否为整数因为即使是number lua_isstring也会通过
            {
                LOG_LUA_PLUGIN_RUNTIME("整数 %d 作为键", lua_tointeger(L, -2));
                this_loop_key = std::to_string(lua_tointeger(L, -2));
            }
            else if (lua_isstring(L, -2))
            {
                LOG_LUA_PLUGIN_RUNTIME("字符串 %s 作为键", lua_tostring(L, -2));
                this_loop_key = std::string(lua_tostring(L, -2));
            }
            else
            {
                LOG_FATAL("lua2protobuf_nostack 不支持以 %s 类型为键", luaL_typename(L, -2));
                lua_pop(L, 1); // field_val
                // key需要保留用于下次遍历
                continue;
            }

            LOG_LUA_PLUGIN_RUNTIME("在 %s 中发现了字段 %s 目前Lua栈大小 %d", frame.package_ptr->GetDescriptor()->name().c_str(), this_loop_key.c_str(), lua_gettop(L));

            const google::protobuf::FieldDescriptor *field = descriptor->FindFieldByName(this_loop_key);
            if (!field)
            {
                LOG_FATAL("在Proto结构 [%s] 中并不能找到字段 [\"%s\"]", descriptor->name().c_str(), this_loop_key.c_str());
                LOG_LUA_PLUGIN_RUNTIME("放弃处理字段 %s:[\"%s\"]", descriptor->name().c_str(), this_loop_key.c_str());
                lua_pop(L, 1); // field_val
                // key需要保留用于下次遍历
                continue;
            }

            switch (field->cpp_type())
            {
            case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
            {
                // 这个key对应的是数组
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        // 获取-1位置的val中的第i个元素后放入栈顶
                        lua_rawgeti(L, -1, arr_idx);
                        // 获取栈顶的值
                        if (lua_isinteger(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%d", lua_tointeger(L, -1));
                            reflection->AddInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                        }
                        // 弹出栈顶的值
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isinteger(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%d", lua_tointeger(L, -1));
                        reflection->SetInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                    }
                }
                // 把栈顶刚才解析出的val弹出
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isinteger(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%lld", lua_tointeger(L, -1));
                            reflection->AddInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isinteger(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%lld", lua_tointeger(L, -1));
                        reflection->SetInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isinteger(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%u", lua_tointeger(L, -1));
                            reflection->AddUInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isinteger(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%u", lua_tointeger(L, -1));
                        reflection->SetUInt32(frame.package_ptr, field, lua_tointeger(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isinteger(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%llu", lua_tointeger(L, -1));
                            reflection->AddUInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isinteger(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%llu", lua_tointeger(L, -1));
                        reflection->SetUInt64(frame.package_ptr, field, lua_tointeger(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isnumber(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%lf", lua_tonumber(L, -1));
                            reflection->AddDouble(frame.package_ptr, field, lua_tonumber(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isnumber(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%lf", lua_tonumber(L, -1));
                        reflection->SetDouble(frame.package_ptr, field, lua_tonumber(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isnumber(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%lf", lua_tonumber(L, -1));
                            reflection->AddFloat(frame.package_ptr, field, lua_tonumber(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isnumber(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%lf", lua_tonumber(L, -1));
                        reflection->SetFloat(frame.package_ptr, field, lua_tonumber(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isboolean(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%d", lua_toboolean(L, -1));
                            reflection->AddBool(frame.package_ptr, field, lua_toboolean(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isboolean(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%d", lua_toboolean(L, -1));
                        reflection->SetBool(frame.package_ptr, field, lua_toboolean(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isstring(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%s", lua_tostring(L, -1));
                            reflection->AddString(frame.package_ptr, field, lua_tostring(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isstring(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%s", lua_tostring(L, -1));
                        reflection->SetString(frame.package_ptr, field, lua_tostring(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
            {
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    const int n_in_array = lua_rawlen(L, -1);
                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, -1, arr_idx);
                        if (lua_isinteger(L, -1))
                        {
                            LOG_LUA_PLUGIN_RUNTIME("%u", lua_tointeger(L, -1));
                            reflection->AddEnumValue(frame.package_ptr, field, lua_tointeger(L, -1));
                        }
                        lua_pop(L, 1);
                    }
                }
                else
                {
                    if (lua_isinteger(L, -1))
                    {
                        LOG_LUA_PLUGIN_RUNTIME("%u", lua_tointeger(L, -1));
                        reflection->SetEnumValue(frame.package_ptr, field, lua_tointeger(L, -1));
                    }
                }
                lua_pop(L, 1); // field_val
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
            {
                // 无论如何lua栈顶都应该是Message
                if (!lua_istable(L, -1))
                {
                    LOG_FATAL("lua2protobuf field[%s] is not a table in package[%s]",
                              this_loop_key.c_str(), descriptor->name().c_str());
                    lua_pop(L, 1); // field_val
                    break;
                }

                // 到这里lua栈顶目前是本次field的 val与key
                // 要创建新的栈帧了
                if (field->is_repeated())
                {
                    if (!lua_istable(L, -1))
                    {
                        LOG_FATAL("Proto目标类型为 数组 lua值非table in %s", this_loop_key.c_str());
                        lua_pop(L, 1); // field_val
                        break;
                    }
                    // arr_item_val
                    // arr_item_val
                    // field_val 等arr_item_val栈帧处理完后需要清除
                    // field_key 等arr_item_val栈帧处理完后需要清除
                    LOG_LUA_PLUGIN_RUNTIME("MESSAGE field->is_repeated()");
                    const int n_in_array = lua_rawlen(L, -1);   // 数组现在放在lua栈顶 读一下数组多少个元素
                    int array_val_in_lua_stack = lua_gettop(L); // 数组就在栈顶

                    if (n_in_array == 0)
                    {
                        lua_pop(L, 1); // 数组长度为0直接把栈顶的table弹出也就是this_loop_key的value
                    }

                    for (int arr_idx = 1; arr_idx <= n_in_array; ++arr_idx)
                    {
                        lua_rawgeti(L, array_val_in_lua_stack, arr_idx); // 会将arr_idx处读的值放到栈顶

                        if (!lua_istable(L, -1))
                        {
                            LOG_FATAL("lua2protobuf field[%s] item is not table in package[%s]", this_loop_key.c_str(), descriptor->name().c_str());
                            lua_pop(L, 1); // 将这个数组元素从lua栈顶弹出
                            continue;      // 放弃处理这个数组元素因为lua传过来的数组元素不可能是Message
                        }

                        google::protobuf::Message *subMessage = reflection->AddMessage(frame.package_ptr, field);

                        LOG_LUA_PLUGIN_RUNTIME("AddMessage subMessage %s", subMessage->GetDescriptor()->name().c_str());

                        int n_in_luastack = lua_gettop(L); // 数组Message元素在lua栈的位置
                        StackFrame new_frame;
                        new_frame.package_ptr = subMessage;
                        new_frame.frame_in_luastack = n_in_luastack;
                        new_frame.curr_key_is_nil = true;
                        new_frame.luapop_num_on_destory = (arr_idx == 1) ? 2 : 1;
                        cpp_stack.push(new_frame);
                        LOG_LUA_PLUGIN_RUNTIME("创建栈帧 %s", new_frame.package_ptr->GetDescriptor()->name().c_str());
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
                    new_frame.curr_key_is_nil = true;
                    new_frame.luapop_num_on_destory = 1; // 需要cpp帧弹出后清理下 val
                    cpp_stack.push(new_frame);
                    LOG_LUA_PLUGIN_RUNTIME("创建栈帧 %s", new_frame.package_ptr->GetDescriptor()->name().c_str());
                }

                break;
            }
            default:
            {
                lua_pop(L, 1); // field_val
                LOG_FATAL("field->cpp_type() %d key[%s] not support in [%s]", (int)field->cpp_type(), this_loop_key.c_str(), descriptor->name().c_str());
                break;
            }
            }
        }
        else // 这帧彻底处理完毕了 没有剩余字段需要遍历了
        {
            LOG_LUA_PLUGIN_RUNTIME("这帧彻底处理完毕了");
            // 函数传过来的创建的首帧
            if (frame.package_ptr == &package)
            {
                LOG_LUA_PLUGIN_RUNTIME("弹出首帧栈帧 field %s", frame.package_ptr->GetDescriptor()->name().c_str());
                cpp_stack.pop();
                // 保留函数调用过lua栈传过来的首个val
            }
            else // while过程中产生的帧
            {
                LOG_LUA_PLUGIN_RUNTIME("弹出中间栈帧 field %s luapop_num_on_destory %d", frame.package_ptr->GetDescriptor()->name().c_str(), (int)frame.luapop_num_on_destory);

                if (frame.luapop_num_on_destory == 1)
                {
                    lua_pop(L, 1); // arr_item_val or field_val
                }
                else if (frame.luapop_num_on_destory == 2)
                {
                    lua_pop(L, 1); // arr_item_val
                    lua_pop(L, 1); // field_val
                }

                cpp_stack.pop();
            }
        }
    }
}

void lua_plugin::protobuf2lua_nostack(lua_State *L, const google::protobuf::Message &package)
{
    struct StackFrame
    {
        google::protobuf::Message *package_ptr{nullptr};
        int next_field_idx{0};
        signed char settable_n_on_pop{0};
        int repeated_loop_idx{0};
    };
    std::stack<StackFrame> cpp_stack;

    {
        StackFrame new_frame;
        new_frame.package_ptr = const_cast<google::protobuf::Message *>(&package);
        new_frame.next_field_idx = 0;
        new_frame.settable_n_on_pop = 0;
        new_frame.repeated_loop_idx = 0;
        lua_newtable(L);
        cpp_stack.push(new_frame);
    }

    while (!cpp_stack.empty())
    {
        StackFrame &frame = cpp_stack.top();

        const google::protobuf::Descriptor *descriptor = frame.package_ptr->GetDescriptor();
        const google::protobuf::Reflection *reflection = frame.package_ptr->GetReflection();

        LOG_LUA_PLUGIN_RUNTIME("frame %s", descriptor->name().c_str());

        if (frame.next_field_idx >= descriptor->field_count())
        {
            ASSERT_LOG_EXIT(frame.settable_n_on_pop == 0 || frame.settable_n_on_pop == -3);
            if (frame.settable_n_on_pop != 0)
            {
                LOG_LUA_PLUGIN_RUNTIME("frame %s destory settable_n_on_pop %d", frame.package_ptr->GetDescriptor()->name().c_str(), (int)frame.settable_n_on_pop);
                lua_settable(L, frame.settable_n_on_pop);
            }

            LOG_LUA_PLUGIN_RUNTIME("弹出 %s", frame.package_ptr->GetDescriptor()->name().c_str());

            cpp_stack.pop();
            continue;
        }
        const google::protobuf::FieldDescriptor *field = descriptor->field(frame.next_field_idx);

        LOG_LUA_PLUGIN_RUNTIME("field %s", field->name().c_str());

        // 将 这个field的key入栈
        if (frame.repeated_loop_idx == 0)
        {
            lua_pushstring(L, field->name().c_str());
        }

        if (!field->is_repeated())
        {
            LOG_LUA_PLUGIN_RUNTIME("!field->is_repeated()");

            frame.next_field_idx++;
            frame.repeated_loop_idx = 0;

            int cpp_type = field->cpp_type();
            switch (cpp_type)
            {
            case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
            {
                google::protobuf::int32 val = reflection->GetInt32(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%d", field->name().c_str(), val);
                lua_pushinteger(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
            {
                google::protobuf::int64 val = reflection->GetInt64(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%lld", field->name().c_str(), val);
                lua_pushinteger(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
            {
                google::protobuf::uint32 val = reflection->GetUInt32(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%u", field->name().c_str(), val);
                lua_pushinteger(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
            {
                google::protobuf::uint64 val = reflection->GetUInt64(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%llu", field->name().c_str(), val);
                lua_pushinteger(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
            {
                double val = reflection->GetDouble(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%lf", field->name().c_str(), val);
                lua_pushnumber(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
            {
                float val = reflection->GetFloat(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%f", field->name().c_str(), val);
                lua_pushnumber(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
            {
                int val = reflection->GetBool(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%d", field->name().c_str(), val);
                lua_pushboolean(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
            {
                std::string val = reflection->GetString(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%s", field->name().c_str(), val.c_str());
                lua_pushlstring(L, val.c_str(), val.size());
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
            {
                google::protobuf::uint32 val = reflection->GetEnumValue(*frame.package_ptr, field);
                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%u", field->name().c_str(), val);
                lua_pushinteger(L, val);
                lua_settable(L, -3);
                break;
            }
            case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
            {
                StackFrame new_frame;
                new_frame.package_ptr = const_cast<google::protobuf::Message *>(
                    &reflection->GetMessage(*frame.package_ptr, field));

                LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s:%s", field->name().c_str(), new_frame.package_ptr->GetDescriptor()->name().c_str());

                new_frame.next_field_idx = 0;
                new_frame.settable_n_on_pop = -3;
                new_frame.repeated_loop_idx = 0;
                lua_newtable(L);
                cpp_stack.push(new_frame);

                break;
            }
            default:
            {
                lua_pushnil(L);
                lua_settable(L, -3);
                LOG_FATAL("field->cpp_type() %d key[%s] not support in [%s]", (int)field->cpp_type(), field->name().c_str(), descriptor->name().c_str());
                break;
            }
            }
            continue;
        }
        else
        {
            LOG_LUA_PLUGIN_RUNTIME("field->is_repeated()");

            // 这个field是一个数组 就是这个field对应的val类型为table
            // 不能直接像 lua_pushnumber 那样入栈需要创建新的表
            if (frame.repeated_loop_idx == 0)
            {
                LOG_LUA_PLUGIN_RUNTIME("frame.repeated_loop_idx == 0 create new table");
                lua_newtable(L);
            }

            int array_size = reflection->FieldSize(*frame.package_ptr, field);
            if (array_size == 0) // 数组为空则之间建一个空表即可
            {
                LOG_LUA_PLUGIN_RUNTIME("空数组");
                lua_settable(L, -3);
                frame.next_field_idx++;
                frame.repeated_loop_idx = 0;
                continue;
            }

            int cpp_type = field->cpp_type();

            bool need_break_goto_new_frame = false;

            while (frame.repeated_loop_idx < array_size)
            {
                LOG_LUA_PLUGIN_RUNTIME("解析数组 %s 内第 %d 个内容 cpp_type %d", field->name().c_str(), frame.repeated_loop_idx + 1, cpp_type);

                lua_pushinteger(L, frame.repeated_loop_idx + 1); // 数组下表做key

                switch (cpp_type)
                {
                case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
                {
                    google::protobuf::int32 val = reflection->GetRepeatedInt32(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%d", field->name().c_str(), frame.repeated_loop_idx, val);
                    lua_pushinteger(L, val);
                    lua_settable(L, -3); // 压入数组元素
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
                {
                    google::protobuf::int64 val = reflection->GetRepeatedInt64(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%lld", field->name().c_str(), frame.repeated_loop_idx, val);
                    lua_pushinteger(L, val);
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
                {
                    google::protobuf::uint32 val = reflection->GetRepeatedUInt32(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%u", field->name().c_str(), frame.repeated_loop_idx, val);
                    lua_pushinteger(L, val);
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
                {
                    google::protobuf::uint64 val = reflection->GetRepeatedUInt64(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%llu", field->name().c_str(), frame.repeated_loop_idx, val);
                    lua_pushinteger(L, val);
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
                {
                    double val = reflection->GetRepeatedDouble(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%lf", field->name().c_str(), frame.repeated_loop_idx, val);
                    lua_pushnumber(L, val);
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
                {
                    float val = reflection->GetRepeatedFloat(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%f", field->name().c_str(), frame.repeated_loop_idx, val);
                    lua_pushnumber(L, val);
                    lua_settable(L, -3);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
                {
                    int bool2int = reflection->GetRepeatedBool(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%d", field->name().c_str(), frame.repeated_loop_idx, bool2int);
                    lua_pushboolean(L, bool2int);
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
                {
                    std::string val = reflection->GetRepeatedString(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%s", field->name().c_str(), frame.repeated_loop_idx, val.c_str());
                    lua_pushlstring(L, val.c_str(), val.size());
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
                {
                    google::protobuf::uint32 val = reflection->GetRepeatedEnumValue(*frame.package_ptr, field, frame.repeated_loop_idx);
                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%u", field->name().c_str(), frame.repeated_loop_idx, val);
                    lua_pushinteger(L, val);
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
                {
                    LOG_LUA_PLUGIN_RUNTIME("数组元素类型为 Message");
                    StackFrame new_frame;
                    new_frame.next_field_idx = 0;

                    new_frame.package_ptr = const_cast<google::protobuf::Message *>(&reflection->GetRepeatedMessage(*frame.package_ptr, field, frame.repeated_loop_idx));

                    LOG_LUA_PLUGIN_RUNTIME("Proto2Lua %s[%d]:%s", field->name().c_str(), frame.repeated_loop_idx, new_frame.package_ptr->GetDescriptor()->name().c_str());

                    frame.repeated_loop_idx++;
                    new_frame.settable_n_on_pop = -3;
                    lua_newtable(L);
                    cpp_stack.push(new_frame);
                    need_break_goto_new_frame = true;
                    break;
                }
                default:
                {
                    LOG_FATAL("field->cpp_type() %d key[%s] not support in [%s]  frame.repeated_loop_idx[%d]",
                              (int)field->cpp_type(),
                              field->name().c_str(),
                              descriptor->name().c_str(),
                              frame.repeated_loop_idx);
                    lua_pushnil(L);
                    lua_settable(L, -3);
                    frame.repeated_loop_idx++;
                    break;
                }
                }

                if (need_break_goto_new_frame)
                {
                    break;
                }
            }

            if (frame.repeated_loop_idx >= array_size)
            {
                if (need_break_goto_new_frame == false)
                {
                    LOG_LUA_PLUGIN_RUNTIME("数组 %s 处理完毕1", field->name().c_str());
                    frame.next_field_idx++;
                    frame.repeated_loop_idx = 0;

                    lua_settable(L, -3);
                    LOG_LUA_PLUGIN_RUNTIME("数组 %s 处理完毕2", field->name().c_str());
                }
            }
        }
    }
}

std::shared_ptr<google::protobuf::Message>
lua_plugin::protobuf_cmd2message(int cmd)
{
    auto it = this->message_factory.find(cmd);
    if (it != this->message_factory.end())
        return it->second();

    return nullptr;
}

// 将C++与Lua需要交互的协议加进来
void lua_plugin::init_message_factory()
{
    this->message_factory[ProtoCmd::PROTO_CMD_LUA_TEST] = []()
    { return std::make_shared<ProtoLuaTest>(); };
}
