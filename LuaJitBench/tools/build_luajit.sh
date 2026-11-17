#!/usr/bin/env bash
# Builds one git ref of the LuaJIT fork into libs/<name>/<target>/.
# Usage: tools/build_luajit.sh <name> <git-ref> <vita|host>
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ $# -ne 3 ]; then
    echo "usage: $0 <name> <git-ref> <vita|host>" >&2
    exit 2
fi
name=$1
ref=$2
target=$3

case $target in
    vita) make_flags=$VITA_MAKE_FLAGS ;;
    host) make_flags="" ;;
    *) echo "target must be vita or host" >&2; exit 2 ;;
esac

cd "$BENCH_DIR"
commit=$(git -C "$FORK_DIR" rev-parse --verify "$ref^{commit}")
src=build/luajit-src-$name-$target
out=libs/$name/$target

step "export $ref ($commit) to $src"
rm -rf "$src"
mkdir -p "$src"
# autocrlf off: the export must keep the LF line ends of the repo.
git -C "$FORK_DIR" -c core.autocrlf=false archive --format=tar "$commit" | tar -x -C "$src"

step "make $name $target"
in_docker "cd /bench/$src && make -j\$(nproc) $make_flags"

step "copy to $out"
rm -rf "$out"
mkdir -p "$out/jit" "libs/$name/include"
cp "$src/src/libluajit.a" "$out/libluajit-5.1.a"
cp "$src"/src/jit/*.lua "$out/jit/"
for h in lua.h lualib.h lauxlib.h luaconf.h lua.hpp luajit.h; do
    cp "$src/src/$h" "libs/$name/include/"
done
{
    echo "ref: $ref"
    echo "commit: $commit"
    echo "target: $target"
    echo "make flags: $make_flags"
} > "$out/SOURCE.txt"

test -f "$out/jit/vmdef.lua"
step "done $out"
