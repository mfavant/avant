local Other = {};
local Log = require("Log");

function Other:OnInit(isHot)
    local log = "OnOtherInit isHot " .. tostring(isHot);
    Log:Error(log);
end

function Other:OnStop(isHot)
    local log = "OnOtherStop isHot " .. tostring(isHot);
    Log:Error(log);
end

function Other:OnTick()
    -- Log:Error("OnOtherTick ");
    -- local t = {
    --     ["num"] = 2,
    --     ["int32array"] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
    --     ["data"] = {
    --         ["str"] = "hello world"
    --     }
    -- };
    -- local res = avant.Lua2Protobuf(t, 8);
    -- if res == nil then
    --     Log:Error("avant.Lua2Protobuf res nil ");
    -- else
    --     Log:Error("avant.Lua2Protobuf res " .. res);
    -- end
end

function Other:OnLuaVMRecvMessage(cmd, message)
end

return Other;
