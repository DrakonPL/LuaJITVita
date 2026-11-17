#!/usr/bin/env bash
# Runs in the image. Builds tools/layout/layout.c with the buildvm flags and with the Vita lib flags
# of one LuaJIT source tree, then compares the struct offsets.
# Usage: tools/layout/check_in_image.sh <luajit src dir> <output dir>
set -euo pipefail
src=$1
out=$2
bench=$(pwd)
layout_c=$bench/tools/layout/layout.c
mkdir -p "$out"

make_flags=(HOST_CC="gcc -m32" CROSS=arm-vita-eabi- TARGET_SYS=PSP2 TARGET_FLAGS="-marm -fno-optimize-sibling-calls")
cd "$src"
host_cmd=$(make -n -B host/buildvm.o "${make_flags[@]}" | grep ' host/buildvm\.c$')
target_cmd=$(make -n -B lj_dispatch.o "${make_flags[@]}" | grep '^arm-vita-eabi-gcc .* lj_dispatch\.c$')

# _BUILDVM_H skips the target checks of lj_arch.h, as in buildvm.
eval "${host_cmd% -c -o *} -D_BUILDVM_H -DLAYOUT_MAIN -o $bench/$out/host_layout $layout_c"
eval "${target_cmd% -c -o *} -I. -S -o $bench/$out/target.s $layout_c"
cd "$bench"
"$out/host_layout" > "$out/host.txt"
python3 tools/layout/compare.py "$out/host.txt" "$out/target.s"
