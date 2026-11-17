#include "bench_api.h"

#include "platform.h"

#include <lauxlib.h>
#include <stdint.h>

typedef int32_t (*BenchCallback)(int32_t value);

// scripts/bench/ljb_ffi.lua declares the same struct with ffi.cdef.
typedef struct BenchApi {
    int32_t (*add)(int32_t a, int32_t b);
    int64_t (*call_n)(BenchCallback callback, int32_t n);
} BenchApi;

static int32_t bench_add(int32_t a, int32_t b)
{
    return a + b;
}

// Calls back into Lua n times. Returns the sum of the results.
static int64_t bench_call_n(BenchCallback callback, int32_t n)
{
    int64_t sum = 0;
    for (int32_t i = 1; i <= n; i++)
        sum += callback(i);
    return sum;
}

static const BenchApi kBenchApi = { bench_add, bench_call_n };

static int lua_bench_add(lua_State *L)
{
    const int32_t a = (int32_t)luaL_checkinteger(L, 1);
    const int32_t b = (int32_t)luaL_checkinteger(L, 2);
    lua_pushinteger(L, bench_add(a, b));
    return 1;
}

// ms since an unknown start, from the platform clock. The profile run of the harness uses it.
static int lua_bench_now_ms(lua_State *L)
{
    lua_pushnumber(L, (lua_Number)plat_time_us() / 1000.0);
    return 1;
}

static int open_bench_api(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, lua_bench_now_ms);
    lua_setfield(L, -2, "now_ms");
    lua_pushcfunction(L, lua_bench_add);
    lua_setfield(L, -2, "add");
    lua_pushlightuserdata(L, (void *)&kBenchApi);
    lua_setfield(L, -2, "api");
    return 1;
}

void bench_api_register(lua_State *L)
{
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, open_bench_api);
    lua_setfield(L, -2, "ljb");
    lua_pop(L, 2);
}
