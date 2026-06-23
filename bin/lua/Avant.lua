---@class avant
---@field Logger function avant.Logger(str):integer
---@field Lua2Protobuf function Client:avant.Lua2Protobuf(message, 1, cmd, clientGID, workerIdx, ""); IPC:avant.Lua2Protobuf(message, 2, cmd, 0, -1, appId); UDP:avant.Lua2Protobuf(message, 3, cmd, 0, port, ip);
---@field CreateNewProtobufByCmd function avant.CreateNewProtobufByCmd(cmd)->table:Message|nil
---@field HighresTime function avant.HighresTime():seconds:number, nanoseconds:integer
---@field Monotonic function avant.Monotonic():steady_clocknanoseconds:integer
---@field LuaDir string LuaDir路径
---@field AppID string 本服务AppID 大区.服.服务ID.实例ID
---@field GetAppID function 返回本服务AppID
---@field GetDBSvrGoAppID function ():string 获取与之对应的DBSvrGO进程的AppID
---@field GetThisServiceAppIDParts function ():list 返回{"大区","服","服务ID","实例ID"}
---@field AVANT_DBSVRGO_APPID string|nil 缓存对应DbSvrGO进程的AppID
---@field AVANT_THISSERVICEINSTANCE_APPID_PARTS table|nil 返回本服务实例的{"大区","服","服务ID","实例ID"}
---@field INT32_MAX integer int32最大值=2147483647
---@field INT32_MIN integer int32最小值=-2147483648
---@field INT64_MAX string int64最大值="9223372036854775807"
---@field INT64_MIN string int64最小值="-9223372036854775808"
---@field UINT32_MAX integer uint32最大值=4294967295
---@field UINT32_MIN integer uint32最小值=0
---@field UINT64_MAX string uint64最大值="18446744073709551615"
---@field UINT64_MIN string uint64最小值="0"
---@field DOUBLE_INTEGER_MAX number double类型精确表示最大整数 9007199254740991
---@field DOUBLE_INTEGER_MIN number double类型精确表示最小整数 -9007199254740992
---@field FLOAT_INTEGER_MAX number float类型精确表示最大整数 8388607
---@field FLOAT_INTEGER_MIN number float类型精确表示最小整数 -8388607

---@type avant
avant                          = avant or {};

avant.INT32_MAX                = 2147483647;
avant.INT32_MIN                = -2147483648;
avant.INT64_MAX                = "9223372036854775807";
avant.INT64_MIN                = "-9223372036854775808";
avant.UINT32_MAX               = 4294967295;
avant.UINT32_MIN               = 0;
avant.UINT64_MAX               = "18446744073709551615";
avant.UINT64_MIN               = "0";
avant.DOUBLE_INTEGER_MAX       = 9007199254740991;
avant.DOUBLE_INTEGER_MIN       = -9007199254740992;
avant.FLOAT_INTEGER_MAX        = 8388607;
avant.FLOAT_INTEGER_MIN        = -8388607;

local AVANT_MAPSVRGO_SERVICEID = "1"
local AVANT_DBSVRGO_SERVICEID  = "2"

---@return table 返回本服务实例的{"大区","服","服务ID","实例ID"}
function avant:GetThisServiceAppIDParts()
    -- 只会在未初始化时计算一次
    if self.AVANT_THISSERVICEINSTANCE_APPID_PARTS == nil then
        local selfAppID = self.AppID -- "大区.服.服务ID.实例ID"

        -- 使用 string.gmatch 分割字符串
        local parts = {}
        for part in string.gmatch(selfAppID, "([^%.]+)") do
            table.insert(parts, part)
        end
        self.AVANT_THISSERVICEINSTANCE_APPID_PARTS = parts
    end

    -- 复制表，确保不会修改原始数据
    local newParts = {}
    for i, v in ipairs(self.AVANT_THISSERVICEINSTANCE_APPID_PARTS) do
        newParts[i] = v
    end

    return newParts
end

---@return string
function avant:GetAppID()
    return self.AppID;
end

---@return string 返回本服务对应的DBSvrGO进程的AppID
function avant:GetDBSvrGoAppID()
    -- 如果缓存已有值，直接返回
    if self.AVANT_DBSVRGO_APPID ~= nil then
        return self.AVANT_DBSVRGO_APPID
    end

    local parts = self:GetThisServiceAppIDParts()

    -- 替换服务ID部分，假设服务ID是第三个部分
    parts[3] = AVANT_DBSVRGO_SERVICEID

    -- 重新拼接成一个字符串
    local newAppID = table.concat(parts, ".")

    self.AVANT_DBSVRGO_APPID = newAppID

    return self.AVANT_DBSVRGO_APPID
end

return avant;
