package.path = avant.LuaDir .. "/?.lua;" .. package.path
package.path = avant.LuaDir .. "/Algorithm/?.lua;" .. package.path
package.path = avant.LuaDir .. "/Debug/?.lua;" .. package.path
package.path = avant.LuaDir .. "/Msg/?.lua;" .. package.path
package.path = avant.LuaDir .. "/Numeric/?.lua;" .. package.path
package.path = avant.LuaDir .. "/ProtoLua/?.lua;" .. package.path
package.path = avant.LuaDir .. "/Time/?.lua;" .. package.path

local avant = require("Avant")
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


---@param isMainVM boolean
---@param isOtherVM boolean
---@param isWorkerVM boolean
---@param workerIdx integer
---@param msg_type integer
---@param cmd integer
---@param message table
---@param uint64_param1_string string
---@param int64_param2_string string
---@param str_param3 string
function OnLuaVMRecvMessage(isMainVM, isOtherVM, isWorkerVM, workerIdx, msg_type, cmd, message, uint64_param1_string,
                            int64_param2_string, str_param3)
    local Other = require("Other")
    if isOtherVM then
        Other:OnLuaVMRecvMessage(msg_type, cmd, message, uint64_param1_string, int64_param2_string, str_param3);
    end
end
