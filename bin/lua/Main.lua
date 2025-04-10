local Main = {};
local Log = require("Log");

function Main:OnInit(isHot)
    Log:Error("OnMainInit isHot %s", tostring(isHot));
end

function Main:OnStop(isHot)
    local log = "OnMainStop isHot " .. tostring(isHot);
    Log:Error(log);
end

function Main:OnTick()
    -- Log:Error("OnMainTick ");
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

function Main:OnLuaVMRecvMessage(cmd, message)
end

return Main;
