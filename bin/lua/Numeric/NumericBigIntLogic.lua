---@class NumericBigInt
NumericBigInt = NumericBigInt or {};
NumericBigInt.__index = NumericBigInt;

-- 配置:每块存储7位数字
-- 注意：不要超过7，否则乘法可能会溢出Lua的double精度(2^53)
local CHUNK_SIZE = 7;
local BASE = 10 ^ CHUNK_SIZE;
---@diagnostic disable-next-line: access-invisible
local unpack = table.unpack or unpack;

--- 构造一个新的NumericBigInt
---@param value NumericBigInt|string|number
---@return NumericBigInt
function NumericBigInt.new(value)
    local self = setmetatable({}, NumericBigInt);

    -- 传入的是一个NumbericBigInt
    if type(value) == "table" and value.__is_bigint then
        -- 浅拷贝
        self.chunks = { unpack(value.chunks) };
        self.sign = value.sign;
    elseif type(value) == "string" then -- 传入的是一个string
        self:FromString(value);
    elseif type(value) == "number" then --传入的是一个number
        self:FromNumber(math.floor(value));
    else                                -- 其他按0处理
        self.chunks = { 0 };
        self.sign = 1;
    end

    self.__is_bigint = true;
    return self;
end

--- 字符串转为NumbericBigInt
---@param str string
---@return nil
function NumericBigInt:FromString(str)
    -- 删除所有空格
    str = str:gsub("%s+", "")
    if str == "" then
        self.chunks = { 0 };
        self.sign = 1;
        return;
    end

    -- 处理符号
    self.sign = 1
    if str:sub(1, 1) == "-" then
        self.sign = -1
        str = str:sub(2)
    elseif str:sub(1, 1) == "+" then
        str = str:sub(2)
    end

    -- 移除前导零
    str = str:gsub("^0+", "")
    if str == "" then str = "0" end

    self.chunks = {}
    local len = #str

    -- 从后往前，每 CHUNK_SIZE 位截取一段
    for i = len, 1, -CHUNK_SIZE do
        local start = math.max(1, i - CHUNK_SIZE + 1)
        local chunkStr = str:sub(start, i)
        table.insert(self.chunks, tonumber(chunkStr))
    end
end

--- 从number:double转NumbericBigInt
---@param num number
---@return nil
function NumericBigInt:FromNumber(num)
    if num == 0 then
        self.chunks = { 0 };
        self.sign = 1;
        return
    end

    self.sign = num < 0 and -1 or 1
    num = math.abs(num)

    self.chunks = {}
    while num > 0 do
        table.insert(self.chunks, num % BASE)
        num = math.floor(num / BASE)
    end
end

--- NumbericBigInt转字符串
---@return string
function NumericBigInt:ToString()
    if #self.chunks == 0 then return "0" end
    if #self.chunks == 1 and self.chunks[1] == 0 then return "0" end

    local parts = {}
    if self.sign == -1 then table.insert(parts, "-") end

    -- 最高位不需要补零
    table.insert(parts, tostring(self.chunks[#self.chunks]))

    -- 剩下的块需要补前导零
    local fmt = "%0" .. CHUNK_SIZE .. "d"
    for i = #self.chunks - 1, 1, -1 do
        table.insert(parts, string.format(fmt, self.chunks[i]))
    end

    return table.concat(parts)
end

--- 原表tostring方法
---@return string
function NumericBigInt:__tostring()
    return self:ToString();
end

--- 绝对值比较运算
---@param other NumericBigInt
---@return number 返回1表示self大于other返回-1表示self小于other返回0表示self大于other
function NumericBigInt:CompareAbs(other)
    if #self.chunks > #other.chunks then return 1 end
    if #self.chunks < #other.chunks then return -1 end

    for i = #self.chunks, 1, -1 do
        if self.chunks[i] > other.chunks[i] then return 1 end
        if self.chunks[i] < other.chunks[i] then return -1 end
    end
    return 0
end

--- 比较大小
---@param other NumericBigInt
---@return number 返回1表示self大于other返回-1表示self小于other返回0表示self大于other
function NumericBigInt:Compare(other)
    if self.sign ~= other.sign then return self.sign end
    return self.sign * self:CompareAbs(other)
end

--- ==比较方法
---@param other NumericBigInt
---@return boolean
function NumericBigInt:__eq(other)
    return self:Compare(other) == 0
end

--- <比较方法
---@param other NumericBigInt
---@return boolean
function NumericBigInt:__lt(other)
    return self:Compare(other) < 0
end

--- <=比较方法
---@param other NumericBigInt
---@return boolean
function NumericBigInt:__le(other)
    return self:Compare(other) <= 0
end

--- 去除高位纯0 chunk
function NumericBigInt:Trim()
    -- 当 chunks 的长度大于 1 且最后一个元素为 0 时，移除 chunks 中的最后一个元素
    while #self.chunks > 1 and self.chunks[#self.chunks] == 0 do
        table.remove(self.chunks)
    end

    -- 如果 chunks 只剩下一个元素并且它的值为 0，则将 sign 设置为 1（表示非负数）
    if #self.chunks == 1 and self.chunks[1] == 0 then
        self.sign = 1 -- 将符号重置为 1，通常 1 表示正数，0 表示负数
    end
end

--- 判断是否为0
---@return boolean
function NumericBigInt:IsZero()
    return #self.chunks == 1 and self.chunks[1] == 0
end

--- 拷贝
---@return NumericBigInt
function NumericBigInt:Clone()
    return NumericBigInt.new(self);
end

--- 两者绝对值相加 abs(self)+abs(other)
---@param other NumericBigInt
---@return NumericBigInt 返回相加后的绝对值结果
function NumericBigInt:AddAbs(other)
    local res = NumericBigInt.new(0)
    res.chunks = {}
    local carry = 0
    local len = math.max(#self.chunks, #other.chunks)

    for i = 1, len do
        local sum = (self.chunks[i] or 0) + (other.chunks[i] or 0) + carry
        if sum >= BASE then
            carry = 1
            sum = sum - BASE
        else
            carry = 0
        end
        table.insert(res.chunks, sum)
    end

    if carry > 0 then table.insert(res.chunks, carry) end
    return res
end

--- 两者绝对值相减 abs(self)-abs(other)
---@param other NumericBigInt
---@return NumericBigInt
function NumericBigInt:SubAbs(other)
    local res = NumericBigInt.new(0)
    res.chunks = {}
    local borrow = 0

    for i = 1, #self.chunks do
        local diff = self.chunks[i] - (other.chunks[i] or 0) - borrow
        if diff < 0 then
            diff = diff + BASE
            borrow = 1
        else
            borrow = 0
        end
        table.insert(res.chunks, diff)
    end

    res:Trim()
    return res
end

--- 两数相加self+other
---@param other NumericBigInt
---@return NumericBigInt
function NumericBigInt:__add(other)
    local b = type(other) == "table" and other or NumericBigInt.new(other)

    if self.sign == b.sign then
        local res = self:AddAbs(b)
        res.sign = self.sign
        return res
    else
        if self:CompareAbs(b) >= 0 then
            local res = self:SubAbs(b)
            res.sign = self.sign
            return res
        else
            local res = b:SubAbs(self)
            res.sign = b.sign
            return res
        end
    end
end

--- 两数相减
---@param other NumericBigInt
---@return NumericBigInt
function NumericBigInt:__sub(other)
    local b = type(other) == "table" and other or NumericBigInt.new(other)
    b = b:Clone()
    b.sign = -b.sign
    ---@diagnostic disable-next-line: return-type-mismatch
    return self + b
end

--- 返回符号位取返后结果
---@return NumericBigInt
function NumericBigInt:__unm()
    local res = self:Clone()
    res.sign = -res.sign
    return res
end

--- 乘法 self*other
---@return NumericBigInt
function NumericBigInt:__mul(other)
    local b = type(other) == "table" and other or NumericBigInt.new(other)

    if self:IsZero() or b:IsZero() then
        return NumericBigInt.new(0)
    end

    local res = NumericBigInt.new(0)
    -- 预分配空间
    for i = 1, #self.chunks + #b.chunks do
        res.chunks[i] = 0
    end

    for i = 1, #self.chunks do
        local carry = 0
        for j = 1, #b.chunks do
            local idx = i + j - 1
            local prod = self.chunks[i] * b.chunks[j] + res.chunks[idx] + carry
            res.chunks[idx] = prod % BASE
            carry = math.floor(prod / BASE)
        end

        -- 处理剩余进位
        if carry > 0 then
            local idx = i + #b.chunks
            res.chunks[idx] = (res.chunks[idx] or 0) + carry
        end
    end

    res.sign = self.sign * b.sign
    res:Trim()
    return res
end

--- 计算self/num
---@param num number
---@return NumericBigInt,number 返回结果和余数
function NumericBigInt:DivSmall(num)
    -- 检查除数是否为 0，避免除零错误
    if num == 0 then
        error("Division by zero")
    end

    -- 计算除数的绝对值
    local absNum = math.abs(num)

    -- 创建一个新的 NumericBigInt 对象用于存储结果
    local res = NumericBigInt.new(0)
    res.chunks = {}

    -- 初始化余数为 0
    ---@type number
    local remainder = 0

    -- 从 self.chunks 的最后一项开始，逐步计算结果
    for i = #self.chunks, 1, -1 do
        -- 计算当前的数字：余数 * 基数 + 当前块的值
        local current = remainder * BASE + self.chunks[i]

        -- 将当前的结果除以 absNum 并将商存入 res.chunks 的开头
        table.insert(res.chunks, 1, math.floor(current / absNum))

        -- 更新余数为当前数字除以 absNum 的余数
        remainder = current % absNum
    end

    -- 设置结果的符号：如果除数为负数，结果的符号取反
    res.sign = self.sign * (num < 0 and -1 or 1)

    -- 去除结果中多余的零
    res:Trim()

    -- 返回结果和余数，余数的符号根据被除数的符号来确定
    return res, remainder * self.sign
end

--- 大整数除法
---@param other NumericBigInt
---@return NumericBigInt
function NumericBigInt:__div(other)
    ---@type NumericBigInt
    local divisor = type(other) == "table" and other or NumericBigInt.new(other)

    -- 除数为 0
    if divisor:IsZero() then
        error("Division by zero")
    end

    -- 被除数为 0
    if self:IsZero() then
        return NumericBigInt.new(0)
    end

    -- 被除数绝对值小于除数
    if self:CompareAbs(divisor) < 0 then
        return NumericBigInt.new(0)
    end

    -- 优化：除数是单块
    if #divisor.chunks == 1 then
        local q, _ = self:DivSmall(divisor.chunks[1] * divisor.sign)
        return q
    end

    -- 符号处理
    local resSign = self.sign * divisor.sign
    local dividend = self:Clone()
    dividend.sign = 1
    divisor = divisor:Clone()
    divisor.sign = 1

    local quotient = NumericBigInt.new(0)
    quotient.chunks = {}
    local remainder = NumericBigInt.new(0)

    -- 从高位到低位处理
    for i = #dividend.chunks, 1, -1 do
        -- 将当前块加入余数
        table.insert(remainder.chunks, 1, dividend.chunks[i])
        remainder:Trim()

        -- 二分查找试商
        local low, high = 0, BASE - 1
        local q = 0

        while low <= high do
            local mid = math.floor((low + high) / 2)
            ---@type NumericBigInt
            local temp = divisor:__mul(NumericBigInt.new(mid));

            if temp:Compare(remainder) <= 0 then
                q = mid
                low = mid + 1
            else
                high = mid - 1
            end
        end

        table.insert(quotient.chunks, 1, q)
        remainder = remainder:__sub(divisor:__mul(NumericBigInt.new(q)));
    end

    quotient.sign = resSign
    quotient:Trim()
    return quotient
end

--- 取模运算
---@param other NumericBigInt
---@return NumericBigInt
function NumericBigInt:__mod(other)
    local divisor = type(other) == "table" and other or NumericBigInt.new(other)

    if divisor:IsZero() then
        error("Modulo by zero")
    end

    -- 优化：除数是单块
    if #divisor.chunks == 1 then
        local _, r = self:DivSmall(divisor.chunks[1] * divisor.sign)
        return NumericBigInt.new(r)
    end

    ---@type NumericBigInt
    local quotient = self:__div(divisor);
    return self:__sub((quotient:__mul(divisor)));
end

--- 幂运算
---@param exp number
---@return NumericBigInt
function NumericBigInt:__pow(exp)
    if type(exp) == "table" then
        error("Exponent must be a number")
    end

    if exp < 0 then
        error("Negative exponent not supported for integers")
    end

    if exp == 0 then
        return NumericBigInt.new(1)
    end

    if exp == 1 then
        return self:Clone()
    end

    -- 快速幂算法
    local result = NumericBigInt.new(1)
    local base = self:Clone()
    local n = exp

    while n > 0 do
        if n % 2 == 1 then
            result = result:__mul(base)
        end
        base = base:__mul(base)
        n = math.floor(n / 2)
    end

    return result
end

--- 返回绝对值
---@return NumericBigInt
function NumericBigInt:Abs()
    local res = self:Clone()
    res.sign = 1
    return res
end

--- GCD最大公约数
---@return NumericBigInt
function NumericBigInt:GCD(other)
    local a = self:Abs()
    local b = (type(other) == "table" and other or NumericBigInt.new(other)):Abs()

    while not b:IsZero() do
        local temp = b
        b = a:__mod(b);
        a = temp
    end

    return a
end

--- 左移（乘以 10^n）
---@param n number
---@return NumericBigInt
function NumericBigInt:ShiftLeft(n)
    if n <= 0 then return self:Clone() end

    local res = self:Clone()
    local fullChunks = math.floor(n / CHUNK_SIZE)
    local partialShift = n % CHUNK_SIZE

    -- 整块移动
    for i = 1, fullChunks do
        table.insert(res.chunks, 1, 0)
    end

    -- 部分位移
    if partialShift > 0 then
        res = res:__mul(NumericBigInt.new(10 ^ partialShift));
    end

    return res
end

--- 右移（除以 10^n）
---@return NumericBigInt
function NumericBigInt:ShiftRight(n)
    if n <= 0 then return self:Clone() end

    local res = self:Clone()
    local fullChunks = math.floor(n / CHUNK_SIZE)
    local partialShift = n % CHUNK_SIZE

    -- 整块移除
    for i = 1, fullChunks do
        if #res.chunks > 1 then
            table.remove(res.chunks, 1)
        else
            res.chunks = { 0 }
            res.sign = 1
            return res
        end
    end

    -- 部分位移
    if partialShift > 0 then
        res = res:__div(NumericBigInt.new(10 ^ partialShift));
    end

    res:Trim()
    return res
end

if not ... then -- 如果是直接运行而非被 require
    print("=== NumericBigInt 测试 ===\n")

    -- 基础运算
    print("--- 基础运算 ---")
    local a = NumericBigInt.new("123456789")
    local b = NumericBigInt.new("987654321")
    print("a =", a)
    print("b =", b)
    print("a + b =", a + b)
    print("a - b =", a - b)
    print("a * b =", a * b)
    print("a / b =", a / b)
    print("a % b =", a % b)
    -- 更大数测试
    print("\n--- 大数测试 ---")
    local c = NumericBigInt.new("123456789012345678901234567890")
    local d = NumericBigInt.new("987654321098765432109876543210")
    print("c =", c)
    print("d =", d)
    print("c + d =", c + d)
    print("d - c =", d - c)
    print("c * d =", c * d)
    print("d / c =", d / c)
    print("d % c =", d % c)

    -- 幂运算测试
    print("\n--- 幂运算 ---")
    local e = NumericBigInt.new("2")
    print("2^10 =", e ^ 10)
    print("2^50 =", e ^ 50)

    -- 负数测试
    print("\n--- 负数测试 ---")
    local f = NumericBigInt.new("-1000000000000000000")
    local g = NumericBigInt.new("333333333333333333")
    print("f =", f)
    print("g =", g)
    print("f + g =", f + g)
    print("f - g =", f - g)
    print("f * g =", f * g)
    print("f / g =", f / g)
    print("f % g =", f % g)

    -- GCD 测试
    print("\n--- GCD 测试 ---")
    local h = NumericBigInt.new("48")
    local i = NumericBigInt.new("180")
    print("gcd(48, 180) =", h:GCD(i))

    -- shift 测试
    print("\n--- shift 测试 ---")
    local j = NumericBigInt.new("123456789")
    print("j =", j)
    print("j << 5 =", j:ShiftLeft(5))  -- * 10^5
    print("j >> 5 =", j:ShiftRight(5)) -- / 10^5

    print("1==1 =", NumericBigInt.new("1") == NumericBigInt.new("1"));
    print("1~=1 =", NumericBigInt.new("1") ~= NumericBigInt.new("1"));
    print("1>=1 =", NumericBigInt.new("1") >= NumericBigInt.new("1"));
    print("1<=1 =", NumericBigInt.new("1") <= NumericBigInt.new("1"));
    print("1>=2 =", NumericBigInt.new("1") >= NumericBigInt.new("2"));
    print("1<=2 =", NumericBigInt.new("1") <= NumericBigInt.new("2"));
    print("1>1 =", NumericBigInt.new("1") > NumericBigInt.new("1"));
    print("1<1 =", NumericBigInt.new("1") < NumericBigInt.new("1"));
    print("1>2 =", NumericBigInt.new("1") > NumericBigInt.new("2"));
    print("1<2 =", NumericBigInt.new("1") < NumericBigInt.new("2"));

    print("NumericBigInt.new(2.132)", NumericBigInt.new(2.132));
    print("NumericBigInt.new(-2.132)", NumericBigInt.new(-2.132));

    print("\n=== 测试结束 ===")
end

return NumericBigInt;
