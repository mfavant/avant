package.path = "./lua/?.lua;" .. package.path
require("log")

function OnMainInit(isHot)
    Log("OnMainInit isHot %s", tostring(isHot));
end

function OnMainStop(isHot)
    local log = "OnMainStop isHot " .. tostring(isHot);
    Log(log);
end

function OnMainTick()
    -- local log = "OnMainTick";
    -- Log(log);
end

function OnWorkerInit(workerIdx, isHot)
    local log = "OnWorkerInit workerIdx " .. workerIdx .. " isHot " ..
                    tostring(isHot);
    Log(log)
end

function OnWorkerStop(workerIdx, isHot)
    local log = "OnWorkerStop" .. workerIdx .. " isHot " .. tostring(isHot);
    Log(log)
end

function OnWorkerTick(workerIdx)
    -- Log("OnWorkerTick " .. workerIdx)
    -- local t = {
    --     ["num"] = 2,
    --     ["int32array"] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
    --     ["data"] = {["str"] = "hello world"}
    -- };
    -- local res = avant.Lua2Protobuf(t, 8);
    -- if res == nil then
    --     Log("avant.Lua2Protobuf res nil " .. workerIdx);
    -- else
    --     Log("avant.Lua2Protobuf res " .. workerIdx .. " " .. res);
    -- end
end

function OnWorkerRecvMessage(workerIdx, cmd, message)
    Log("avant.OnWorkerRecvMessage " .. workerIdx .. " " .. cmd .. " ");
    if cmd == 8 then
        local num = message["num"];
        local int32array = message["int32array"];
        local int32array_str = table.concat(int32array, " ")
        local data = message["data"];
        local data_str = data["str"];
        Log("avant.OnWorkerRecvMessage num " .. num .. " " .. "int32array " ..
                int32array_str .. " " .. data_str);
    else
        Log("avant.OnWorkerRecvMessage cmd != 8");
    end
end

function OnOtherInit(isHot)
    local log = "OnOtherInit isHot " .. tostring(isHot);
    Log(log);
end

function OnOtherStop(isHot)
    local log = "OnOtherStop isHot " .. tostring(isHot);
    Log(log);
end

function OnOtherTick()
    -- local log = "OnOtherTick";
    -- Log(log)
end
