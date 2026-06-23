---@class TimeMgr
local TimeMgr = require("TimeMgrData")

---@return number 返回纳秒排除微秒级部分时间戳
function TimeMgr.GetNS()
    local seconds, milliseconds, nanoseconds = avant.HighresTime()
    return nanoseconds
end

---@return integer 返回毫秒时间戳
function TimeMgr.GetMS()
    local seconds, milliseconds, nanoseconds = avant.HighresTime()
    local integer_part, number_part = math.modf(milliseconds)
    return integer_part;
end

---@return number 返回秒时间戳
function TimeMgr.GetS()
    local seconds, milliseconds, nanoseconds = avant.HighresTime()
    return seconds
end

return TimeMgr;
