local Other = {};
local Log = require("Log");

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

Other.OnLuaVMRecvMessageCnt = 0;

local ProtoCmd_PROTO_CMD_LUA_TEST = 8;

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
    local t1 = {
        ["num"] = 2,
        ["int32array"] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
        ["data"] = {
            ["str"] = "hello world",
            ["strarray"] = { "hello", "world", "this", "is", "avant" },
            ["inner"] = {
                ["double_val"] = 1.1,
                ["float_val"] = 1.0,
                ["int32_val"] = 99,
                ["int64_val"] = 110,
                ["uint32_val"] = 111,
                ["uint64_val"] = 112,
                ["bool_val"] = true,
                ["str_val"] = "this is str_val"
            },
            ["inner_array"] = { {
                ["double_val"] = 1.2,
                ["float_val"] = 1.1,
                ["int32_val"] = 100,
                ["int64_val"] = 111,
                ["uint32_val"] = 112,
                ["uint64_val"] = 113,
                ["bool_val"] = true,
                ["str_val"] = "this is str_val 1"
            }, {
                ["double_val"] = 1.3,
                ["float_val"] = 1.2,
                ["int32_val"] = 101,
                ["int64_val"] = 112,
                ["uint32_val"] = 113,
                ["uint64_val"] = 114,
                ["bool_val"] = true,
                ["str_val"] = "this is str_val 2"
            }, {
                ["double_val"] = 1.4,
                ["float_val"] = 1.3,
                ["int32_val"] = 102,
                ["int64_val"] = 113,
                ["uint32_val"] = 114,
                ["uint64_val"] = 115,
                ["bool_val"] = true,
                ["str_val"] = "this is str_val 3"
            } }
        }
    };
    local t2 = {
        ["num"] = nil,
        ["int32array"] = { 1 },
        ["data"] = {
            ["strarray"] = {},
            ["inner"] = {}
        }
    };
    local t3 = {};
    -- 勘误inner下面这样写会崩溃
    local t4 = {
        -- ["num1"] = "cds",
        ["int32array"] = { "as" },
        ["data"] = {
            -- ["strarray"] = {1},
            ["inner"] = { "cds", "vfd" }
        }
    };

    -- local res = avant.Lua2Protobuf(t1, ProtoCmd_PROTO_CMD_LUA_TEST);
    -- if res == nil then
    --     -- Log:Error("avant.Lua2Protobuf failed");
    -- else
    --     -- Log:Error("avant.Lua2Protobuf succ " .. res);
    -- end

    -- Create New ProtoLuaTest message table
    -- local protoLuaTest = avant.CreateNewProtobufByCmd(ProtoCmd_PROTO_CMD_LUA_TEST);
    -- if protoLuaTest == nil then
    --     Log:Error("avant.CreateNewProtobufByCmd(ProtoCmd_PROTO_CMD_LUA_TEST) return nil");
    -- else
    --     Log:Error("avant.CreateNewProtobufByCmd(ProtoCmd_PROTO_CMD_LUA_TEST) return %s", DebugTableToString(protoLuaTest));
    -- end
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
    self.OnLuaVMRecvMessageCnt = self.OnLuaVMRecvMessageCnt + 1;
    if (math.maxinteger == self.OnLuaVMRecvMessageCnt) then
        self.OnLuaVMRecvMessageCnt = 0;
    end
    if (self.OnLuaVMRecvMessageCnt % 10000 == 0) then
        Log:Error("self.OnLuaVMRecvMessageCnt %d", self.OnLuaVMRecvMessageCnt);
    end

    if cmd == ProtoCmd_PROTO_CMD_LUA_TEST then
        -- Log:Error("OnLuaVMRecvMessage cmd[%d] %s", cmd, DebugTableToString(message));
    end
end

return Other;
