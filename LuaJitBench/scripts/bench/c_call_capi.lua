-- n calls of add(i, 1) into C through a lua_CFunction (src/bench_api.c).
-- A trace cannot call a lua_CFunction directly. LuaJIT 2.1 can stitch the trace around the call.
local add = require("ljb").add

local function run(n)
  local sum = 0
  for i = 1, n do
    sum = sum + add(i, 1)
  end
  return sum
end

return { name = "c_call_capi", run = run, n = 1000000 }
