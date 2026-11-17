-- From LuaJIT-test-cleanup/bench/recursive-fib.lua (see README.md). Only the driver at the end is new.
local function fib(n)
  if n < 2 then return 1 end
  return fib(n-2) + fib(n-1)
end

local function run(n)
  return fib(n)
end

return { name = "fib", run = run, n = 31 }
