local Main = {};
local Log = require("Log");

function Main:OnInit()
    Log:Error("OnMainInit");
end

function Main:OnStop()
    local log = "OnMainStop";
    Log:Error(log);
end

function Main:OnTick()
    -- Log:Error("OnMainTick ");
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

function Main:OnReload()
    Log:Error("luavm Main:OnReload");
end

function Main:OnLuaVMRecvMessage(cmd, message)
    -- ProtoCmd::PROTO_CMD_LUA_TEST = 8;
    -- if cmd == 8 then
    --     Log:Error("OnLuaVMRecvMessage cmd[%d]", cmd);
    -- end
end

return Main;
