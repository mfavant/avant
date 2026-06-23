---@class MapSvrType
---@field safeStop boolean 安全停服是否已经被触发
---@field IsSafeStop function 返回boolean安全停服是否已经被触发

---@class MapSvr:MapSvrType
MapSvr = MapSvr or {
    safeStop = false
};

require("ProtoLuaImport");
local Log = require("Log");
local MsgHandler = require("MsgHandlerLogic")
local TimeMgr = require("TimeMgrLogic")
local AlgorithmRandom = require("AlgorithmRandomLogic")

--- 线程被启动时只调用一次
---@return nil
function MapSvr.OnInit()
    AlgorithmRandom.RandomSeed(math.floor(TimeMgr.GetS()))
    MapSvr.safeStop = false;
end

--- 线程被终止时触发一次
---@return nil
function MapSvr.OnStop()
end

--- 线程Tick频率以配置文件变量epoll_wait_time控制
---@return nil
function MapSvr.OnTick()
end

--- 安全停服，需要在OnStop前被调用，做一些清理工作
---@return nil
function MapSvr.OnSafeStop()
    Log:Error("MapSvr.OnSafeStop()");
    MapSvr.safeStop = true;
end

--- 是否已经进行了安全停服
---@return boolean
function MapSvr.IsSafeStop()
    return MapSvr.safeStop;
end

--- 当服务器进程被kill -SIGUSR1 或 线程Init时被触发
---@return nil
function MapSvr.OnReload()
    -- hot load PlayerLogic scriptss
    local reloadList = {}
    table.insert(reloadList, "PlayerLogic")
    table.insert(reloadList, "MsgHandlerFromUDPLogic")
    table.insert(reloadList, "MsgHandlerFromOtherLogic")
    table.insert(reloadList, "MsgHandlerFromClientLogic")
    table.insert(reloadList, "MsgHandlerLogic")
    table.insert(reloadList, "TimeMgrLogic")

    for i, name in ipairs(reloadList) do
        package.loaded[name] = nil;
    end

    ---@type table<integer,string>
    local reloadErrorList = {};

    for i, name in ipairs(reloadList) do
        local ok, module = pcall(require, name)
        if ok then
            Log:Error("%s.lua Reloaded", name);
        else
            Log:Error("%s.lua Reload Err %s", name, tostring(module))
            table.insert(reloadErrorList, name);
        end
    end

    MsgHandler:OnReload();

    local reloadErrorListStr = "MapSvr.OnReload Error List:[";
    for i, name in ipairs(reloadErrorList) do
        reloadErrorListStr = reloadErrorListStr .. name;
        if i ~= #reloadErrorList then
            reloadErrorListStr = reloadErrorListStr .. ",";
        end
    end
    reloadErrorListStr = reloadErrorListStr .. "]";
    Log:Error("%s", reloadErrorListStr);

    Log:Error("MapSvr.OnReload Done");
end

--- 当C++给Lua虚拟机传递新的Protobuf消息
---@param msg_type integer
---@param cmd integer
---@param message table
---@param uint64_param1_string string
---@param int64_param2_string string
---@param str_param3 string
---@return nil
function MapSvr.OnLuaVMRecvMessage(msg_type,
                                   cmd,
                                   message,
                                   uint64_param1_string,
                                   int64_param2_string,
                                   str_param3)
    -- Log:Error("OnLuaVMRecvMessage cmd[%d] uint64_param1_string[%s] int64_param2_string[%s] str_param3[%s]", cmd, uint64_param1_string, int64_param2_string, str_param3)
    if msg_type == ProtoLua_ProtoLuaVMMsgType.PROTO_LUA_VM_MSG_TYPE_CLIENT then -- 客户端
        local clientGID = uint64_param1_string
        local workerIdx = tonumber(int64_param2_string)
        if workerIdx == nil then
            Log:Error("OnLuaVMRecvMessage workerIdx == nil");
            return;
        end
        MsgHandler:HandlerMsgFromClient(clientGID, workerIdx, cmd, message);
    elseif msg_type == ProtoLua_ProtoLuaVMMsgType.PROTO_LUA_VM_MSG_TYPE_IPC then -- ipc
        MsgHandler:HandlerMsgFromOther(cmd, message, str_param3);
    elseif msg_type == ProtoLua_ProtoLuaVMMsgType.PROTO_LUA_VM_MSG_TYPE_UDP then -- udp
        MsgHandler:HandlerMsgFromUDP(cmd, message, str_param3, int64_param2_string);
    else
        Log:Error("OnLuaVMRecvMessage Unknown msg_type %d", msg_type)
    end
end

return MapSvr;
