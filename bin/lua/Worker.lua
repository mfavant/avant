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
    --         ["str"] = "hello world"
    --     }
    -- };
    -- local res = avant.Lua2Protobuf(t, 8);
    -- if res == nil then
    --     Log:Error("avant.Lua2Protobuf res nil " .. workerIdx);
    -- else
    --     Log:Error("avant.Lua2Protobuf res " .. workerIdx .. " " .. res);
    -- end
end

function Worker:OnReload(workerIdx)
    Log:Error("luavm Worker:OnReload workerIdx " .. tostring(workerIdx));
end

function Worker:OnLuaVMRecvMessage(workerIdx, cmd, message)
end

return Worker;
