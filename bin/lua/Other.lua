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
end

return Other;
