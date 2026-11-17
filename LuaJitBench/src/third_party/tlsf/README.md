# TLSF

`tlsf.c` and `tlsf.h` are copies of [mattconte/tlsf](https://github.com/mattconte/tlsf), commit `deff9ab5` (2020-03-29). The license is BSD 3-clause (see the header of `tlsf.h`). Do not change these files.

`src/arena_alloc.c` uses TLSF for the `nolock` allocator (`ALLOC=nolock`). The Makefiles build `tlsf.c` with `-w`, because it is not our code.
