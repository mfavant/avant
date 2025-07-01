package.path = avant.LuaDir .. "/?.lua;" .. package.path
package.path = avant.LuaDir .. "/Player/?.lua;" .. package.path

local Log = require("Log")
local Main = require("Main")
local Worker = require("Worker")
local Other = require("Other")

function OnMainInit()
    Main:OnInit();
end

function OnMainStop()
    Main:OnStop();
end

function OnMainTick()
    Main:OnTick();
end

function OnMainReload()
    Main:OnReload();
end

function OnWorkerInit(workerIdx)
    Worker:OnInit(workerIdx);
end

function OnWorkerStop(workerIdx)
    Worker:OnStop(workerIdx);
end

function OnWorkerTick(workerIdx)
    Worker:OnTick(workerIdx);
end

function OnWorkerReload(workerIdx)
    Worker:OnReload(workerIdx);
end

function OnOtherInit()
    Other:OnInit();
end

function OnOtherStop()
    Other:OnStop();
end

function OnOtherTick()
    Other:OnTick();
end

function OnOtherReload()
    Other:OnReload();
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
