local Log = {};

function Log:Error(...)
    local args = {...};
    local formatString = table.remove(args, 1);

    local info = debug.getinfo(2, "Sl");
    local source = info.source;
    local line = info.currentline;

    local message = string.format(formatString, table.unpack(args));

    avant.Logger(string.format("[%s:%d] %s", source, line, message));
end

return Log;
