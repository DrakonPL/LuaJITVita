#ifndef LJB_BENCH_API_H
#define LJB_BENCH_API_H

#include <lua.h>

// Adds package.preload["ljb"]: the C side of the benches c_call_capi and c_call_ffi.
//   ljb.now_ms()  - the platform clock in ms
//   ljb.add(a, b) - a lua_CFunction
//   ljb.api       - a light userdata that points to struct ljb_api (see scripts/bench/ljb_ffi.lua)
void bench_api_register(lua_State *L);

#endif
