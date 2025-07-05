local Other = {};
local Log = require("Log");

function Other:OnInit()
    local log = "OnOtherInit";
    Log:Error(log);
    Other:OnReload();
end

function Other:OnStop()
    local log = "OnOtherStop";
    Log:Error(log);
end

function Other:OnTick()
    PlayerLogic.Move(1001, 1, 1);
    -- Log:Error("OnOtherTick ");
    local t = {
        ["num"] = 2,
        ["int32array"] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
        ["data"] = {
            ["str"] = "hello world",
            ["strarray"] = {"hello", "world", "this", "is", "avant"}
        }
    };
    -- ProtoCmd::PROTO_CMD_LUA_TEST = 8;
    local res = avant.Lua2Protobuf(t, 8);
    if res == nil then
        -- Log:Error("avant.Lua2Protobuf failed");
    else
        -- Log:Error("avant.Lua2Protobuf succ " .. res);
    end
end

-- kill -10 {avant PID}
function Other:OnReload()
    Log:Error("luavm Other:OnReload");

    local reloadList = {}
    table.insert(reloadList, "PlayerLogic")

    for i, name in ipairs(reloadList) do
        package.loaded[name] = nil;
        local ok, module = pcall(require, name)
        if ok then
            Log:Error("%s.lua Reloaded", name);
        else
            Log:Error("%s.lua Reload Err %s ", tostring(module or ""));
        end
    end

end

function Other:OnLuaVMRecvMessage(cmd, message)
    function DebugTableToString(t, indent)
        if type(t) == "string" then
            return "\"" .. tostring(t) .. "\""
        end

        if type(t) ~= "table" then
            return tostring(t)
        end

        indent = indent or 0
        local prefix = string.rep("  ", indent)
        local str = "{\n"
        for k, v in pairs(t) do
            local key = tostring(k)
            local valueStr
            if type(v) == "table" then
                valueStr = DebugTableToString(v, indent + 1)
            else
                valueStr = DebugTableToString(v)
            end
            str = str .. prefix .. "  [\"" .. key .. "\"] = " .. valueStr .. ",\n"
        end
        str = str .. prefix .. "}"
        return str
    end

    -- ProtoCmd::PROTO_CMD_LUA_TEST = 8;
    if cmd == 8 then
        -- Log:Error("OnLuaVMRecvMessage cmd 8 %s", DebugTableToString(message));
    end
end

return Other;
