package.path = avant.LuaDir .. "/?.lua;" .. package.path
package.path = avant.LuaDir .. "/Player/?.lua;" .. package.path

local Log = require("Log")

function OnMainInit()
    local Main = require("Main")
    Main:OnInit();
end

function OnMainStop()
    local Main = require("Main")
    Main:OnStop();
end

function OnMainTick()
    local Main = require("Main")
    Main:OnTick();
end

function OnMainReload()
    local Main = require("Main")
    Main:OnReload();
end

function OnWorkerInit(workerIdx)
    local Worker = require("Worker")
    Worker:OnInit(workerIdx);
end

function OnWorkerStop(workerIdx)
    local Worker = require("Worker")
    Worker:OnStop(workerIdx);
end

function OnWorkerTick(workerIdx)
    local Worker = require("Worker")
    Worker:OnTick(workerIdx);
end

function OnWorkerReload(workerIdx)
    local Worker = require("Worker")
    Worker:OnReload(workerIdx);
end

function OnOtherInit()
    local Other = require("Other")
    Other:OnInit();
end

function OnOtherStop()
    local Other = require("Other")
    Other:OnStop();
end

function OnOtherTick()
    local Other = require("Other")
    Other:OnTick();
end

function OnOtherReload()
    local Other = require("Other")
    Other:OnReload();
end

function OnLuaVMRecvMessage(isMainVM, isOtherVM, isWorkerVM, workerIdx, cmd, message)
    -- Log:Error("avant.OnLuaVMRecvMessage " .. tostring(isMainVM) .. " " .. tostring(isOtherVM) .. " " ..
    --               tostring(isWorkerVM) .. " " .. workerIdx .. " " .. cmd .. " ");

    if isMainVM then
        local Main = require("Main")
        Main:OnLuaVMRecvMessage(cmd, message);
    elseif isOtherVM then
        local Other = require("Other")
        Other:OnLuaVMRecvMessage(cmd, message);
    elseif isWorkerVM then
        local Worker = require("Worker")
        Worker:OnLuaVMRecvMessage(workerIdx, cmd, message);
    end
end
