local Worker = {};
local Log = require("Log");

function Worker:OnInit(workerIdx)
    local log = "OnWorkerInit workerIdx " .. workerIdx;
    Log:Error(log);
end

function Worker:OnStop(workerIdx)
    local log = "OnWorkerStop" .. workerIdx;
    Log:Error(log);
end

function Worker:OnTick(workerIdx)
    -- Log:Error("OnWorkerTick " .. workerIdx);
    -- local t = {
    --     ["num"] = 2,
    --     ["int32array"] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
    --     ["data"] = {
    --         ["str"] = "hello world",
    --         ["strarray"] = {"hello", "world", "this", "is", "avant"}
    --     }
    -- };
    -- -- ProtoCmd::PROTO_CMD_LUA_TEST = 8;
    -- local res = avant.Lua2Protobuf(t, 8);
    -- if res == nil then
    --     Log:Error("avant.Lua2Protobuf failed");
    -- else
    --     Log:Error("avant.Lua2Protobuf succ " .. res);
    -- end
end

function Worker:OnReload(workerIdx)
    Log:Error("luavm Worker:OnReload workerIdx " .. tostring(workerIdx));
end

function Worker:OnLuaVMRecvMessage(workerIdx, cmd, message)
    -- ProtoCmd::PROTO_CMD_LUA_TEST = 8;
    -- if cmd == 8 then
    --     Log:Error("OnLuaVMRecvMessage cmd[%d]", cmd);
    -- end
end

return Worker;
