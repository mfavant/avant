#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include "proto/proto_util.h"
#include "workers/other.h"

#ifdef AVANT_JIT_VERSION
#include "LuaJIT-2.1.ROLLING/src/lua.hpp"
#elif defined(AVANT_NO_JIT_VERSION)
extern "C"
{
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}
#endif

namespace avant::app
{
    class lua_plugin
    {
    public:
        lua_plugin();
        ~lua_plugin();

        void init_message_factory();

        void on_main_init(const std::string &lua_dir, const std::string &app_id, const int worker_cnt);

        void real_on_main_init();
        void on_main_stop();
        void on_main_tick();
        void exe_OnMainInit();
        void exe_OnMainStop();
        void exe_OnMainTick();
        void exe_OnMainReload();

        void on_worker_init(int worker_idx);
        void on_worker_stop(int worker_idx);
        void on_worker_tick(int worker_idx);
        void exe_OnWorkerInit(int worker_idx);
        void exe_OnWorkerStop(int worker_idx);
        void exe_OnWorkerTick(int worker_idx);
        void exe_OnWorkerReload(int worker_idx);

        static void exe_OnLuaVMRecvMessage(lua_State *lua_state,
                                           int msg_type,
                                           int cmd,
                                           const google::protobuf::Message &package,
                                           uint64_t uint64_param1,
                                           int64_t int64_param2,
                                           const std::string &str_param3);

        void on_other_lua_vm_recv_client_message(int cmd,
                                                 const google::protobuf::Message &package,
                                                 uint64_t gid,
                                                 int worker_idx);
        void on_other_lua_vm_recv_ipc_message(int cmd,
                                              const google::protobuf::Message &package,
                                              const std::string &app_id);
        void on_other_lua_vm_recv_udp_message(int cmd,
                                              const google::protobuf::Message &package,
                                              const std::string &from_ip,
                                              int from_port);

        void on_other_init(avant::workers::other *ptr_other_obj);
        void on_other_stop();
        void on_other_tick();
        void exe_OnOtherInit();
        void exe_OnOtherStop();
        void exe_OnOtherTick();
        void exe_OnOtherReload();

        void main_mount();
        void worker_mount(int worker_idx);
        void other_mount();

        void reload();

    private:
        static void lua_plugin_lua_return_not_is_ok_print_error(int isok, lua_State *lua_state);
        static int lua_plugin_lua_error_handler(lua_State *L);
        static int lua_plugin_push_lua_error_handler(lua_State *L);

    public:
        static int Logger(lua_State *lua_state);
        static int Lua2Protobuf(lua_State *lua_state);
        static int CreateNewProtobufByCmd(lua_State *lua_state);
        static int HighresTime(lua_State *lua_state);
        static int Monotonic(lua_State *lua_state);

    public:
        std::shared_ptr<google::protobuf::Message> protobuf_cmd2message(int cmd);
        static void protobuf2lua_nostack(lua_State *L, const google::protobuf::Message &package);
        static void lua2protobuf_nostack(lua_State *L, const google::protobuf::Message &package);

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

        avant::workers::other *ptr_other_obj{nullptr};

        std::string lua_dir;
        std::string app_id;

        std::unordered_map<int, std::function<std::shared_ptr<google::protobuf::Message>()>> message_factory;
    };
}
