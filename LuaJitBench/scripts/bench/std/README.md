# The Standard Benches

These benches come from `bench/` of [LuaJIT-test-cleanup](https://github.com/LuaJIT/LuaJIT-test-cleanup), commit `014708bc` (2016-10-19).

## The License

The repo has no license file, and the bench files have no license header. Most of these programs come from The Computer Language Benchmarks Game. Find the license before this repo gets a public remote.

## The Changes

Each module keeps the original code up to the driver. The driver is the code that reads `arg` and writes the result with `io.write`. A new `run(n)` driver replaces it and returns a checksum.

| Module | Source | Checksum | Other change |
|---|---|---|---|
| `fib` | `recursive-fib.lua` | `fib(n)` | None |
| `nbody` | `nbody.lua` | The sum of the two printed energies, with 6 decimals | None |
| `spectral_norm` | `spectral-norm.lua` | The printed norm, with 6 decimals | None |
| `mandelbrot` | `mandelbrot.lua` | The sum of the bytes of each row | The whole program is the driver loop. The loop is the same. The write of each row became a sum. |
| `fannkuch` | `fannkuch.lua` | `fannkuch(n)` | A local `io` stub drops the 30 permutations that the algorithm writes |
| `binary_trees` | `binary-trees.lua` | The sum of the printed check values | None |
| `ray` | `ray.lua` | The sum of the pixel values | The level is 4, as in `PARAM_arm.txt`. `n` is the image size. |

The plan named `richards`. The repo has no `richards`, so `ray` replaces it. `ray` also calls methods through metatables (`setmetatable` and `__index`).

## The Values of n

Each `n` gives about 30 to 75 ms in the host interpreter. On the Vita, the interpreter ran `trivial` 22 times slower than on the host. So each bench takes about 0.6 to 1.6 s in the Vita interpreter. LJV-4 corrects the values with the device numbers.
