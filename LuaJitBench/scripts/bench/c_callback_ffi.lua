-- C calls a Lua function n times through an FFI callback (src/bench_api.c).
-- The callback code lives in its own VM block on the Vita. The JIT cannot compile the C side.
local ffi = require("ffi")
local api = require("bench.ljb_ffi")

local callback = ffi.cast("ljb_callback", function(value)
  return value + 1
end)

local function run(n)
  return tonumber(api.call_n(callback, n))
end

return { name = "c_callback_ffi", run = run, n = 1000000 }
