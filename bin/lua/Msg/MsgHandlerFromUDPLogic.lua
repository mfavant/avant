local MsgHandler = require("MsgHandlerLogic")
local MapSvr = require("MapSvr");

---@type table<number,function>
MsgHandlerFromUDP = {};

---@param message ProtoLua_ProtoCSReqExample
MsgHandlerFromUDP[ProtoLua_ProtoCmd.PROTO_CMD_CS_REQ_EXAMPLE] = function(cmd, message, ip, port)
    ---@type ProtoLua_ProtoCSResExample
    local t = {
        testContext = message.testContext
    };

    MsgHandler:Send2UDP(ip, port, ProtoLua_ProtoCmd.PROTO_CMD_CS_RES_EXAMPLE, t);
end

---@param message ProtoLua_ProtoUDPSafeStopReq
MsgHandlerFromUDP[ProtoLua_ProtoCmd.PROTO_CMD_UDP_SAFESTOP_REQ] = function(cmd, message, ip, port)
    ---@type ProtoLua_ProtoUDPSafeStopRes
    local t = {
        appId = avant:GetAppID()
    };
    MsgHandler:Send2UDP(ip, port, ProtoLua_ProtoCmd.PROTO_CMD_UDP_SAFESTOP_RES, t);
    MapSvr.OnSafeStop();
end

return MsgHandlerFromUDP;
