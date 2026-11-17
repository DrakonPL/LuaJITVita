#!/usr/bin/env bash
# Shows if a Vita lib has the JIT and the Vita code-memory calls.
# Usage: tools/check_vita_lib.sh <name>
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ $# -ne 1 ]; then
    echo "usage: $0 <name>" >&2
    exit 2
fi
name=$1
cd "$BENCH_DIR"

in_docker "
set -e
lib=libs/$name/vita/libluajit-5.1.a
echo '-- $name'
echo \"version: \$(arm-vita-eabi-strings -a \$lib | grep -m 1 '^LuaJIT 2')\"
jit=\$(arm-vita-eabi-nm --defined-only \$lib | grep -c ' T lj_asm_trace\$' || true)
echo \"JIT compiled in (lj_asm_trace defined): \$([ \$jit -gt 0 ] && echo yes || echo no)\"
echo 'Vita calls (undefined in lj_mcode.o and lj_ccallback.o):'
for o in lj_mcode.o lj_ccallback.o; do
    arm-vita-eabi-nm -u \$lib 2>/dev/null | awk -v o=\$o '/:\$/ {cur=\$1} cur==o\":\" && /sceKernel/ {print \"  \" o \": \" \$2}'
done
echo \"size total (text data bss): \$(arm-vita-eabi-size -t \$lib | tail -n 1 | awk '{print \$1, \$2, \$3}')\"
"
