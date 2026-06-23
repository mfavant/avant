local MsgHandler = require("MsgHandlerLogic")

---@type table<number,function>
MsgHandlerFromOther = {};

---@param message ProtoLua_ProtoCSReqExample
MsgHandlerFromOther[ProtoLua_ProtoCmd.PROTO_CMD_CS_REQ_EXAMPLE] = function(cmd, message, app_id)
    ---@type ProtoLua_ProtoCSResExample
    local t = {
        testContext = message.testContext
    }

    -- 原逻辑是 Send2IPC 但发送的是 message，而不是 t
    MsgHandler:Send2IPC(app_id, ProtoLua_ProtoCmd.PROTO_CMD_CS_RES_EXAMPLE, message)
end

return MsgHandlerFromOther;
