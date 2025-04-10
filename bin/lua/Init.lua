package.path = "./lua/?.lua;" .. package.path
local Log = require("Log")
local Main = require("Main")
local Worker = require("Worker")
local Other = require("Other")

function OnMainInit(isHot)
    Main:OnInit(isHot);
end

function OnMainStop(isHot)
    Main:OnStop(isHot);
end

function OnMainTick()
    Main:OnTick();
end

function OnWorkerInit(workerIdx, isHot)
    Worker:OnInit(workerIdx, isHot);
end

function OnWorkerStop(workerIdx, isHot)
    Worker:OnStop(workerIdx, isHot);
end

function OnWorkerTick(workerIdx)
    Worker:OnTick(workerIdx);
end

function OnOtherInit(isHot)
    Other:OnInit(isHot);
end

function OnOtherStop(isHot)
    Other:OnStop(isHot);
end

function OnOtherTick()
    Other:OnTick();
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

        if isMainVM then
            Main:OnLuaVMRecvMessage(cmd, message);
        elseif isOtherVM then
            Other:OnLuaVMRecvMessage(cmd, message);
        elseif isWorkerVM then
            Worker:OnLuaVMRecvMessage(workerIdx, cmd, message);
        end
    end

end
