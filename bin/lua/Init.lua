package.path = "./lua/?.lua;" .. package.path
local Log = require("Log")

function OnMainInit(isHot)
    Log:Error("OnMainInit isHot %s", tostring(isHot));
end

function OnMainStop(isHot)
    local log = "OnMainStop isHot " .. tostring(isHot);
    Log:Error(log);
end

function OnMainTick()
    -- Log:Error("OnMainTick ")
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

function OnWorkerInit(workerIdx, isHot)
    local log = "OnWorkerInit workerIdx " .. workerIdx .. " isHot " .. tostring(isHot);
    Log:Error(log)
end

function OnWorkerStop(workerIdx, isHot)
    local log = "OnWorkerStop" .. workerIdx .. " isHot " .. tostring(isHot);
    Log:Error(log)
end

function OnWorkerTick(workerIdx)
    -- Log:Error("OnWorkerTick " .. workerIdx)
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

function OnLuaVMRecvMessage(isMainVM, isOtherVM, isWorkerVM, workerIdx, cmd, message)
    Log:Error("avant.OnLuaVMRecvMessage " .. tostring(isMainVM) .. " " .. tostring(isOtherVM) .. " " ..
                  tostring(isWorkerVM) .. " " .. workerIdx .. " " .. cmd .. " ");
    if cmd == 8 then
        local num = message["num"];
        local int32array = message["int32array"];
        local int32array_str = table.concat(int32array, " ")
        local data = message["data"];
        local data_str = data["str"];
        Log:Error("avant.OnLuaVMRecvMessage num " .. num .. " " .. "int32array " .. int32array_str .. " " .. data_str);
    else
        Log:Error("avant.OnLuaVMRecvMessage cmd != 8");
    end
end

function OnOtherInit(isHot)
    local log = "OnOtherInit isHot " .. tostring(isHot);
    Log:Error(log);
end

function OnOtherStop(isHot)
    local log = "OnOtherStop isHot " .. tostring(isHot);
    Log:Error(log);
end

function OnOtherTick()
    -- Log:Error("OnOtherTick ")
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
