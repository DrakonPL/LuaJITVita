#!/usr/bin/env bash
# Checks that buildvm (host gcc -m32) and the Vita compiler see the same LuaJIT struct offsets.
# A difference makes the assembler VM read the wrong fields. Run it after tools/build_luajit.sh.
# Usage: tools/check_layout.sh <name>
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ $# -ne 1 ]; then
    echo "usage: $0 <name>" >&2
    exit 2
fi
name=$1
cd "$BENCH_DIR"
in_docker "bash tools/layout/check_in_image.sh build/luajit-src-$name-vita/src build/layout-$name"
