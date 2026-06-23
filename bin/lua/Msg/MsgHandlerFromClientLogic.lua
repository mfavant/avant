local Log = require("Log")
local MsgHandler = require("MsgHandlerLogic")
local MapSvr = require("MapSvr")

---@type table<number, function>
MsgHandlerFromClient = {}

-- 有新的客户端连接
---@param message ProtoLua_ProtoTunnelWorker2OtherEventNewClientConnection
MsgHandlerFromClient[ProtoLua_ProtoCmd.PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_NEW_CLIENT_CONNECTION] = function (
    playerId, clientGID, workerIdx, cmd, message
)
    if message.gid ~= clientGID then
        Log:Error(
            'PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_NEW_CLIENT_CONNECTION message["gid"]%s ~= clientGID[%s]', message.gid,
            clientGID
        )
        return
    end
    -- Log:Error("New Client Connection gid[%s] workerIdx[%d]", clientGID, workerIdx)

    -- -- 主动关闭客户端的连接
    -- ---@type ProtoLua_ProtoTunnelOtherLuaVM2WorkerCloseClientConnection
    -- local ProtoTunnelOtherLuaVM2WorkerCloseClientConnection = { gid = clientGID, workerIdx = workerIdx }
    -- MsgHandler:Send2Client(
    --     clientGID, workerIdx, ProtoLua_ProtoCmd.PROTO_CMD_TUNNEL_OTHERLUAVM2WORKER_CLOSE_CLIENT_CONNECTION,
    --     ProtoTunnelOtherLuaVM2WorkerCloseClientConnection
    -- )
end

-- 客户端连接关闭
---@param message ProtoLua_ProtoTunnelWorker2OtherEventCloseClientConnection
MsgHandlerFromClient[ProtoLua_ProtoCmd.PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_CLOSE_CLIENT_CONNECTION] = function (
    playerId, clientGID, workerIdx, cmd, message
)
    if message.gid ~= clientGID then
        Log:Error(
            'PROTO_CMD_TUNNEL_WORKER2OTHER_EVENT_CLOSE_CLIENT_CONNECTION message["gid"]%s ~= clientGID[%s]', message.gid,
            clientGID
        )
        return
    end
    -- Log:Error("Close Client Connection gid[%s] workerIdx[%d]", clientGID, workerIdx)
end

-- 示例请求处理
---@param message ProtoLua_ProtoCSReqExample
MsgHandlerFromClient[ProtoLua_ProtoCmd.PROTO_CMD_CS_REQ_EXAMPLE] = function (
    playerId, clientGID, workerIdx, cmd, message
)
    ---@type ProtoLua_ProtoCSResExample
    local t = { testContext = message.testContext }
    MsgHandler:Send2Client(clientGID, workerIdx, ProtoLua_ProtoCmd.PROTO_CMD_CS_RES_EXAMPLE, t)
end

return MsgHandlerFromClient;
