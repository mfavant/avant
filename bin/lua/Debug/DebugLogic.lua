local Debug = require("DebugData")

function Debug:DebugTableToString(t, indent)
    if type(t) == "string" then
        return "\"" .. tostring(t) .. "\""
    end

    if type(t) ~= "table" then
        return tostring(t)
    end

    indent = indent or 0
    local prefix = string.rep("  ", indent)
    local str = "{\n"
    for k, v in pairs(t) do
        local key = tostring(k)
        local valueStr
        if type(v) == "table" then
            valueStr = Debug:DebugTableToString(v, indent + 1)
        else
            valueStr = Debug:DebugTableToString(v)
        end
        str = str .. prefix .. "  [\"" .. key .. "\"] = " .. valueStr .. ",\n"
    end
    str = str .. prefix .. "}"
    return str
end

return Debug

-- 设置你想要断下来的断点行
-- local breakpoints = {
--     [66] = true, -- 比如我要在第 66 行设置断点
--     [70] = true -- 也可以多个断点
-- }
-- local function DebugTableToString(t, indent)
--     if type(t) ~= "table" then
--         return tostring(t)
--     end
--     indent = indent or 0
--     local prefix = string.rep("  ", indent)
--     local str = "{\n"
--     for k, v in pairs(t) do
--         local key = tostring(k)
--         local valueStr
--         if type(v) == "table" then
--             valueStr = DebugTableToString(v, indent + 1)
--         else
--             valueStr = DebugTableToString(v)
--         end
--         str = str .. prefix .. "  [" .. key .. "] = " .. valueStr .. ",\n"
--     end
--     str = str .. prefix .. "}"
--     return str
-- end
-- -- hook 函数：每执行一行都会进来
-- local function DebugDebuggerHook(event, line)
--     if breakpoints[line] then
--         Log:Error("%s", string.format("🛑 断点触发：当前在第 %d 行", line))
--         -- 输出当前所有局部变量
--         local i = 1
--         while true do
--             local name, value = debug.getlocal(2, i)
--             if not name then
--                 break
--             end
--             Log:Error("%s", string.format("局部变量[%s] = %s", name, tableToString(value)))
--             i = i + 1
--         end
--         -- 简单的交互式 shell（也可以做更强大的命令行调试）
--         -- io.write("调试模式，按回车继续 > ")
--         -- io.read()
--     end
-- end
-- debug.sethook(DebugDebuggerHook, "l")
-- 关闭钩子（不再调试）
-- debug.sethook()
