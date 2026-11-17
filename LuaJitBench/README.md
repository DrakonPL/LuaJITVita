# LuaJitBench

LuaJitBench is a small standalone test app. It finds out if the LuaJIT JIT works on the PS Vita. The results are in the `README.md` of the repo root.

## Requirements

- Docker Desktop. It must run before you use the tools.
- Git Bash on Windows.
- This folder inside the LuaJIT repo. The tools build LuaJIT from the git refs of the parent folder. Set `FORK_DIR` to use a different LuaJIT repo.

## The LuaJIT Source

The tools use these refs of the LuaJIT repo:

| Ref | Content |
|---|---|
| `ljv/v2.1` | Upstream LuaJIT v2.1 (github.com/LuaJIT/LuaJIT) with the PSP2 port on top. This is the main source. |
| `c329ddd` | The old SonicMastr fork (LuaJIT 2.1.0-beta3 of 2022). The vitasdk package used this commit. It is the control. To get it, run `git fetch https://github.com/SonicMastr/LuaJIT-Vita v2.1`. |
| `v2.1` of github.com/LuaJIT/LuaJIT | Upstream LuaJIT v2.1, with no Vita code |

Upstream LuaJIT alone cannot use the JIT on the Vita. It turns the JIT off for `__psp2__`. For the vitasdk (`__vita__`), it has no way to get executable memory. The PSP2 port adds the VM memblock calls (`sceKernelAllocMemBlockForVM` and the VM domain calls).

## The Docker Image

All builds run in the image `luajitbench-vita` (`docker/Dockerfile`). The image is Debian with `gcc-multilib`, the vitasdk 2026.08, `taihen`, and `vita-rss-libdl`.

- The `vitasdk/vitasdk` image is Alpine. Alpine has no `gcc -m32`, and LuaJIT needs 32-bit host tools for a 32-bit target.
- The 2026.08 channel has no `luajit`, `taihen`, or `vita-rss-libdl` package. The Dockerfile builds `taihen` and `vita-rss-libdl` from the sources of the old package recipes.

To make the image, run this command:

```sh
tools/build_image.sh
```

## The Libs

The libs are not in git. Build them before the first app build. Each lib is in `libs/<name>/<target>/`. Each target folder has `libluajit-5.1.a`, the `jit/*.lua` modules, and `SOURCE.txt`. The headers are in `libs/<name>/include/`.

| Name | Source | Targets |
|---|---|---|
| `rebase` | `ljv/v2.1` | `vita`, `host` |
| `fork` | `c329ddd` | `vita` |

The `host` target is a Linux x64 build. It gives the reference numbers.

To make the libs, run these commands:

```sh
tools/build_luajit.sh rebase ljv/v2.1 vita
tools/build_luajit.sh rebase ljv/v2.1 host
tools/build_luajit.sh fork c329ddd vita
```

To show if a Vita lib has the JIT and the Vita calls, run this command:

```sh
tools/check_vita_lib.sh rebase
```

To check that `buildvm` and the Vita compiler see the same LuaJIT struct offsets, run this command after a Vita lib build:

```sh
tools/check_layout.sh rebase
```

The exit code is 1 if an offset differs. A difference makes the assembler VM read the wrong fields. The `fork` lib fails this check.

To compare the symbols and the sizes of two Vita libs, run this command:

```sh
tools/compare_libs.sh fork rebase
```

## The Test App

The app is a plain vitasdk C app. It links only LuaJIT and the vitasdk libs. The same runner also builds for Linux x64 (the host build).

| File | Job |
|---|---|
| `src/main.c` | The start, the header lines, the menu, and the button loop |
| `src/probe.c` | Vita only: the VM block probe |
| `src/runner.c` | The Lua state, the modes, the repeats, the median, the checksum check, and the CSV rows |
| `src/platform.h` | The platform functions: time, screen lines, buttons, the CSV file, and the heap block |
| `src/platform_vita.c`, `src/platform_host.c` | The Vita side and the Linux side |
| `src/mcode_wrap.c` | Vita only: counts the VM blocks of LuaJIT and keeps their addresses |
| `src/third_party/` | The `debugScreen` code of the vitasdk samples |
| `scripts/benches.lua` | The list of the benches, in order |
| `scripts/harness.lua` | Sets the JIT mode and counts the trace events in each Lua state |
| `scripts/bench/*.lua` | The benches. Each returns a table: `name`, `run(n)`, `n`. `run` returns a checksum. |

To make a Vita `.vpk`, run this command:

```sh
tools/make.sh -f Makefile.vita LIB=rebase
```

- `LIB=fork` uses the control lib.
- `BIG_HEAP=1` sets the newlib heap to 128 MB. The default is 32 MB.
- `UNSAFE=1` makes an unsafe `eboot.bin`. It needs "Enable Unsafe Homebrew" in the HENkaku settings. The default build is safe (`vita-make-fself -s`).
- The output is `build/vpk/LuaJitBench_<lib>[_bigheap][_unsafe].vpk`. The title ID is `LJBN00001`.

To run the host build, run this command:

```sh
tools/make.sh -f Makefile.host run
```

The host build writes `results/host/results_rebase_host.csv`. Its exit code is 1 if a bench has ERROR or MISMATCH.

### On the Vita

At each start, the app runs the VM block probe for two block sizes (64 KB and 1 MB). It does this with no heap block, then with the 128 MB heap block. Each probe writes `mov r0, #42` and `bx lr` into a VM block and calls it. The screen shows each step before the app does it. If the app crashes, the last line shows the step.

| Button | Action |
|---|---|
| Cross | Runs all benches |
| Square | Turns the 128 MB heap block on or off |
| Triangle | The VM free test: 64 times, allocates a 1 MB VM block and frees it with `sceKernelFreeMemBlock` |
| Circle | The same test with `sceKernelFreeMemBlockForVM` |
| Start | Stops the app |

A free that does not release the block fills the VM space for the rest of the process. So do one free test for each start of the app, before the benches. The test writes `results_<lib>_free_test[_for_vm].csv`.

The app writes `ux0:data/LuaJitBench/results_<lib>_<heap>[_unsafe].csv`. The heap names are `small` and `block` in the default build, and `bigheap` and `bigheap_block` in the `BIG_HEAP=1` build.

### The CSV File

The lines that start with `#` are the header and the notes. The header has the probe results, the LuaJIT version, `jit.status()`, the CPU clock, the lib, and the heap. Each bench gets one row for each mode:

`lib, heap, bench, mode, median_ms, min_ms, max_ms, checksum, traces, aborts, top_abort, mcode_kb, vm_blocks, max_distance_mb`

- `traces`, `aborts`, `top_abort`, `mcode_kb`, and the VM block columns come from the last run of the mode.
- `checksum` is `MISMATCH:<value>` if a run gave a different checksum than the first run of the bench. It is `ERROR` if a run failed.
- `max_distance_mb` is the largest distance from the far end of a VM block to `lj_vm_exit_handler`. Over 14 MB, a trace cannot jump back to the VM.
- The `# run` line after each row lists the address and the size of each VM block. It also gives the count of failed VM block calls, with the last error and size.
