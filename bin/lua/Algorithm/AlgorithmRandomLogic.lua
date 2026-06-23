---@class AlgorithmRandom
AlgorithmRandom = AlgorithmRandom or {}

---@param m integer
---@param n integer
---@return integer
function AlgorithmRandom.Random(m, n)
    return math.random(m, n);
end

---@param seed integer
function AlgorithmRandom.RandomSeed(seed)
    math.randomseed(seed);
end

-- 直接运行时的测试代码
if not ... then                           -- 如果是直接运行而非被 require
    AlgorithmRandom.RandomSeed(os.time()) -- 设置种子
end

-- 返回模块
return AlgorithmRandom;
