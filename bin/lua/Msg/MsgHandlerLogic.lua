---@class MsgHandlerType

---@class MsgHandler:MsgHandlerType
local MsgHandler = require("MsgHandlerData");

--- 发送协议到客户端
---@param clientGID string 客户端连接gid
---@param workerIdx number 客户端连接所在worker下标
---@param cmd number 协议号
---@param message table protobufMessage
function MsgHandler:Send2Client(clientGID, workerIdx, cmd, message)
    avant.Lua2Protobuf(message, ProtoLua_ProtoLuaVMMsgType.PROTO_LUA_VM_MSG_TYPE_CLIENT, cmd, clientGID, workerIdx, "");
end

--- 发送协议到其他进程
---@param appId string 远程进程appid
---@param cmd number 协议号
---@param message table protobufMessage
function MsgHandler:Send2IPC(appId, cmd, message)
    avant.Lua2Protobuf(message, ProtoLua_ProtoLuaVMMsgType.PROTO_LUA_VM_MSG_TYPE_IPC, cmd, 0, -1, appId);
end

--- 发送UDP数据
---@param ip string 目标UDP字符串
---@param port number 目标UDP端口
---@param cmd number 协议号
---@param message table protobufMessage
function MsgHandler:Send2UDP(ip, port, cmd, message)
    avant.Lua2Protobuf(message, ProtoLua_ProtoLuaVMMsgType.PROTO_LUA_VM_MSG_TYPE_UDP, cmd, 0, port, ip);
end

--- MsgHandler被重载后调用
function MsgHandler:OnReload()
    ---@type table<number,function>
    MsgHandler.MsgFromUDPCmd2Func = require("MsgHandlerFromUDPLogic");
    ---@type table<number,function>
    MsgHandler.MsgFromOtherCmd2Func = require("MsgHandlerFromOtherLogic");
    ---@type table<number,function>
    MsgHandler.MsgFromClientCmd2Func = require("MsgHandlerFromClientLogic");
end

--- 客户端来新消息了
---@param clientGID string 客户端连接gid
---@param workerIdx number 客户端连接所在worker下标
---@param cmd integer 协议号
---@param message table protobufMessage
function MsgHandler:HandlerMsgFromClient(clientGID, workerIdx, cmd, message)
    local playerId = tostring(clientGID) .. "_" .. tostring(workerIdx);

    -- 执行对应的 handler（默认什么都不做）
    ---@type any
    local fn = MsgHandler.MsgFromClientCmd2Func[cmd]
    if fn ~= nil then
        return fn(playerId, clientGID, workerIdx, cmd, message)
    end
end

--- 其他进程来新消息了
---@param cmd integer 协议号
---@param message_from_other table 协议
---@param app_id string 从哪个进程appid来的消息
function MsgHandler:HandlerMsgFromOther(cmd, message_from_other, app_id)
    ---@type any
    local fn = MsgHandler.MsgFromOtherCmd2Func[cmd];
    if fn ~= nil then
        return fn(cmd, message_from_other, app_id);
    end
end

--- 接收到了新的UDP消息
---@param cmd number 协议号
---@param message_from_udp table 协议
---@param ip string UDP客户端的IP
---@param port string UDP客户端的端口
function MsgHandler:HandlerMsgFromUDP(cmd, message_from_udp, ip, port)
    ---@type any
    local fn = MsgHandler.MsgFromUDPCmd2Func[cmd];
    if fn ~= nil then
        return fn(cmd, message_from_udp, ip, port);
    end
end

return MsgHandler;
