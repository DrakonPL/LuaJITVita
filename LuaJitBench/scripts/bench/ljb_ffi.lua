-- The FFI view of struct BenchApi in src/bench_api.c. Keep the two the same.
-- ffi.C is not used: on the Vita it needs dlsym on the app.
local ffi = require("ffi")

ffi.cdef [[
typedef int32_t (*ljb_callback)(int32_t value);
typedef struct ljb_api {
  int32_t (*add)(int32_t a, int32_t b);
  int64_t (*call_n)(ljb_callback callback, int32_t n);
} ljb_api;
]]

return ffi.cast("const ljb_api *", require("ljb").api)
