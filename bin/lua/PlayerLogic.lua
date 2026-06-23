-- PlayerLogic.lua logic script, reloadable
local Log = require("Log")
---@class Player
---@field playerId string
---@field x        integer
---@field y        integer
local Player = require("PlayerData")

---@param playerId string
---@param x        integer
---@param y        integer
---@return Player
function Player.new(playerId, x, y)
    ---@type Player
    local self = setmetatable({}, Player)
    self.playerId = playerId
    self.x = x
    self.y = y
    return self
end

---@param xSize integer
---@param ySize integer
function Player:Move(xSize, ySize)
    self.x = self.x + xSize
    self.y = self.y + ySize

    if self.x < 0 or self.x > 1000 then
        self.x = 0
    end
    if self.y < 0 or self.y > 1000 then
        self.y = 0
    end

    ---@type number, integer
    local seconds, nanoseconds = avant.HighresTime()
    ---@type integer
    local steady_clocknanoseconds = avant.Monotonic()

    Log:Error(
        "seconds " .. tostring(seconds) .. " nanoseconds " .. tostring(nanoseconds) .. " steady_clocknanoseconds "
            .. tostring(steady_clocknanoseconds)
    )
    Log:Error("player " .. tostring(self.playerId) .. " x " .. tostring(self.x) .. " y " .. tostring(self.y))
end

return Player
