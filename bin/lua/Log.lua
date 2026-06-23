local Log = {};

---@diagnostic disable-next-line: access-invisible
-- lua5.4 and lua5.1 for luajit
local unpack = table.unpack or unpack;

function Log:Error(...)
    local args = { ... };
    if #args == 0 then
        return;
    end

    local formatString = table.remove(args, 1);
    if not formatString then
        return;
    end

    local info = debug.getinfo(2, "Sl");
    if info == nil then
        return;
    end

    local source = info.source;
    local line = info.currentline;

    local message = string.format(formatString, unpack(args));

    avant.Logger(string.format("[%s:%d] %s", source, line, message));
end

return Log;
