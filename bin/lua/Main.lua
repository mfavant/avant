local Main = {};
local Log = require("Log");

function Main:OnInit()
    Log:Error("OnMainInit");
end

function Main:OnStop()
    local log = "OnMainStop";
    Log:Error(log);
end

function Main:OnTick()

end

function Main:OnReload()
    Log:Error("luavm Main:OnReload");
end

return Main;
