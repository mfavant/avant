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

function OnOtherInit()
    avant.Logger("OnOtherInit")
end

function OnOtherStop()
    avant.Logger("OnOtherStop")
end

function OnOtherTick()
    -- avant.Logger("OnOtherTick")
end
