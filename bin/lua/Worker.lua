local Worker = {};
local Log = require("Log");

function Worker:OnInit(workerIdx)
    local log = "OnWorkerInit workerIdx " .. workerIdx;
    Log:Error(log);
end

function Worker:OnStop(workerIdx)
    local log = "OnWorkerStop" .. workerIdx;
    Log:Error(log);
end

function Worker:OnTick(workerIdx)

end

function Worker:OnReload(workerIdx)
    Log:Error("luavm Worker:OnReload workerIdx " .. tostring(workerIdx));
end

return Worker;
