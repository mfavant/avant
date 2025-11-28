-- PlayerLogic.lua logic script, reloadable
local Log = require("Log");
local PlayerData = require("PlayerData");
PlayerLogic = PlayerLogic or {};

function PlayerLogic.Move(playerId, newX, newY)
    local player = PlayerData[playerId];
    if player then
        if player.pos.x < 0 or player.pos.x > 1000 then
            player.pos.x = 0;
        end

        if player.pos.y < 0 or player.pos.y > 1000 then
            player.pos.y = 0;
        end

        player.pos.x = player.pos.x + newX;
        player.pos.y = player.pos.y + newY;

        local sec, ns = avant.HighresTime();
        local mono = avant.Monotonic();
        -- Log:Error("sec " .. tostring(sec) .. " ns " .. tostring(ns) .. " mono " .. tostring(mono));

        -- Log:Error("player " .. tostring(playerId) .. " x " .. tostring(player.pos.x) .. " y " .. tostring(player.pos.y));
        -- Log:Error("player %d name %s ", player.id, player.name);
    end
end

return PlayerLogic;
