# LuaJIT for the PS Vita

This is upstream [LuaJIT](https://luajit.org/) v2.1 with a port to the PS Vita homebrew SDK ([vitasdk](https://vitasdk.org/)). **The JIT compiler works on the device.** Traces compile, run, and give the same results as on a PC.

The work branch is `ljv/v2.1`. It is upstream v2.1 (`63c8f1a9`) with these changes on top:

1. The PSP2 port of [SonicMastr/LuaJIT-Vita](https://github.com/SonicMastr/LuaJIT-Vita) (commits `c8601a74` to `c329ddd1`). This port adds the OS `LUAJIT_OS_PSP2` for `__vita__`, and the VM memblock calls for the machine code.
2. Five fixes that device tests found. Without them, the JIT compiles no trace on the Vita, or crashes at the first trace.
3. The profiler (`jit.profile`, `jit.p`) on the Vita.

The plain README of LuaJIT is in `README`. The license is the MIT license of LuaJIT. See `COPYRIGHT`.

## The Fixes

The old SonicMastr fork is the source of the vitasdk `luajit` package. It has each fault in this table except the first. So, with that package, the JIT compiles no trace, and a hot loop with the JIT on can be 35 times slower than in the interpreter.

| Fault | Symptom on the device | Fix |
|---|---|---|
| Upstream writes the 64 KB area size into `JIT_PARAMDEF`, so the PSP2 size of 1 MB had no effect | Each VM block call fails with `0x80024B05` (`ILLEGAL_MEMBLOCK_SIZE`). A VM block must be 1 MB or more. | `JIT_PARAMDEF` uses the PSP2 defaults |
| `sceKernelFreeMemBlock` returns success for a VM block, but does not release it | After about 14 blocks, each call fails with `0x80024B0B` (`MEMBLOCK_OVERFLOW`). The VM space is about 16 MB. | Free VM blocks with `sceKernelFreeMemBlockForVM` |
| A second `sceKernelOpenVMDomain` (or close) fails with `0x80010058` | Blocks are allocated, but no code area is set | One VM domain state for the process, shared by the JIT and the FFI callbacks |
| `arm-vita-eabi-gcc` uses short enums. The host `gcc -m32` that builds `buildvm` does not. | Data abort in `lj_BC_JLOOP` at the first trace: the assembler VM reads `jit_State` fields 4 bytes off | Build the host tools with `-fshort-enums` for PSP2 |
| `lj_def.h` tests `LJ_TARGET_PSP2` before `lj_arch.h` defines it | `ffi.cast` of a Lua function fails with "too many callbacks" (the callback block is 4 KB) | Set the 1 MB callback block in `lj_ccallback.c` |

Two more changes:

- The kernel ignores the address hint of a VM block. So the code area allocator makes one try, not 16.
- The profiler uses a pthread timer thread. The app must link `-lpthread`.

## Results

PS Vita at the default 333 MHz, median of three runs. The gain is the interpreter time divided by the JIT time.

| Bench | Interpreter ms | JIT ms | Gain |
|---|---|---|---|
| `spectral-norm` | 3081 | 282 | 10.9 |
| `mandelbrot` | 2804 | 345 | 8.1 |
| `fannkuch` | 3503 | 570 | 6.1 |
| `nbody` | 2027 | 379 | 5.4 |
| `fib` | 1770 | 342 | 5.2 |
| `ray` | 2699 | 564 | 4.8 |
| `binary-trees` | 2031 | 1562 | 1.3 |
| 1,000,000 FFI calls into C | 2564 | 83 | 31.1 |
| 1,000,000 `lua_CFunction` calls | 578 | 536 | 1.1 |

The benches come from `LuaJIT-test-cleanup/bench`. Each result is the same as on the PC, in both modes. The heap size of the app has no effect: the JIT works with a 32 MB and with a 128 MB newlib heap.

## Build

You need the vitasdk, and a host `gcc` that can build 32-bit programs (`gcc-multilib` on Debian). For `dlfcn.h`, you also need the vitasdk packages `vita-rss-libdl` and `taihen`.

To build the static lib, run this command in `src/`:

```sh
make HOST_CC="gcc -m32" CROSS=arm-vita-eabi- TARGET_SYS=PSP2 \
     TARGET_FLAGS="-marm -fno-optimize-sibling-calls"
```

The output is `src/libluajit.a`. The headers are `lua.h`, `lualib.h`, `lauxlib.h`, `luaconf.h`, `lua.hpp`, and `luajit.h` in `src/`.

## Use in an App

1. Link the lib and these libs: `-ldl -ltaihen_stub -lSceKernelModulemgr_stub -lSceSblSsMgr_stub -lSceLibKernel_stub -lSceSysmodule_stub -lpthread -lm`.
2. Put `src/jit/*.lua` (with `vmdef.lua`, which the build makes) into the `.vpk` at `app0:jit/`. Then add `app0:?.lua` to `package.path`. `jit.util`, `jit.v`, and `jit.p` need these files.
3. Use FFI for hot calls into C. In a trace, an FFI call takes about 0.08 µs, and a `lua_CFunction` call about 0.54 µs. With the JIT off, the FFI call is the slower one.
4. Do not use `ffi.C` for functions of the app. Give Lua a struct of function pointers as a light userdata, and cast it with `ffi.cast`.

## Limits

- The VM space for the whole process is about 16 MB. LuaJIT takes up to 8 MB for code (`maxmcode`) and 1 MB for the FFI callbacks.
- Only one thread may run JIT code at a time. The VM domain has one state for the whole process.
- At 333 MHz, the trace compiler is not free. In a 2 s profile of `ray`, it took 7 % of the time.

## Credits

- LuaJIT: Mike Pall, MIT license.
- The PSP2 port: [SonicMastr/LuaJIT-Vita](https://github.com/SonicMastr/LuaJIT-Vita).
