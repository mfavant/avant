function OnMainInit() avant.Logger("OnMainInit") end

function OnMainStop() avant.Logger("OnMainStop") end

function OnMainTick()
    -- avant.Logger("OnMainTick")
end

function OnWorkerInit(workerIdx)
    local log = "OnWorkerInit" .. workerIdx
    avant.Logger(log)
end

function OnWorkerStop(workerIdx)
    local log = "OnWorkerStop" .. workerIdx
    avant.Logger(log)
end

function OnWorkerTick(workerIdx)
    -- local t = {["num"] = 2, ["int32array"] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
    -- local res = avant.Lua2Protobuf(t, 8);
    -- if res == nil then
    --     avant.Logger("avant.Lua2Protobuf res nil " .. workerIdx);
    -- else
    --     avant.Logger("avant.Lua2Protobuf res " .. workerIdx .. " " .. res);
    -- end
end

function OnWorkerRecvMessage(workerIdx, cmd, message)
    avant.Logger("avant.OnWorkerRecvMessage " .. workerIdx .. " " .. cmd .. " ");
    if cmd == 8 then
        local num = message["num"];
        local int32array = message["int32array"];
        local int32array_str = table.concat(int32array, " ")
        avant.Logger("avant.OnWorkerRecvMessage num " .. num .. " " ..
                         "int32array " .. int32array_str);
    else
        avant.Logger("avant.OnWorkerRecvMessage cmd != 8");
    end
end

function OnOtherInit() avant.Logger("OnOtherInit") end

function OnOtherStop() avant.Logger("OnOtherStop") end

function OnOtherTick()
    -- avant.Logger("OnOtherTick")
end
