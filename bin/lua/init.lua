function OnMainInit()
    avant.Logger("OnMainInit")
end

function OnMainStop()
    avant.Logger("OnMainStop")
end

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
    -- local log = "OnWorkerTick" .. workerIdx
    -- avant.Logger(log)
end
