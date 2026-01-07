local Log = {};

-- 判断 Lua 版本
local unpack = table.unpack or unpack;

function Log:Error(...)
    local args = {...};
    local formatString = table.remove(args, 1);

    local info = debug.getinfo(2, "Sl");
    local source = info.source;
    local line = info.currentline;

    local message = string.format(formatString, unpack(args));  -- 根据 Lua 版本使用 unpack 或 table.unpack

    avant.Logger(string.format("[%s:%d] %s", source, line, message));
end

return Log;
