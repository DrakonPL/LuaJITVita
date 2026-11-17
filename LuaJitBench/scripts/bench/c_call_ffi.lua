-- n calls of add(i, 1) into C through an FFI function pointer (src/bench_api.c).
-- The JIT can compile this call into the trace.
local add = require("bench.ljb_ffi").add

local function run(n)
  local sum = 0
  for i = 1, n do
    sum = sum + add(i, 1)
  end
  return sum
end

return { name = "c_call_ffi", run = run, n = 1000000 }
