#!/usr/bin/env bash
# Compares the defined symbols and sizes of two Vita libs.
# The full symbol diff goes to build/compare/<a>-vs-<b>/symbols.diff.
# Usage: tools/compare_libs.sh <name-a> <name-b>
set -euo pipefail
source "$(dirname "$0")/common.sh"

if [ $# -ne 2 ]; then
    echo "usage: $0 <name-a> <name-b>" >&2
    exit 2
fi
a=$1
b=$2
cd "$BENCH_DIR"

in_docker "
set -e
out=build/compare/$a-vs-$b
rm -rf \$out && mkdir -p \$out
for n in $a $b; do
    lib=libs/\$n/vita/libluajit-5.1.a
    arm-vita-eabi-nm --defined-only \$lib | awk 'NF==3 {print \$2, \$3}' | sort -u > \$out/\$n.symbols
    arm-vita-eabi-size -t \$lib | tail -n 1 > \$out/\$n.size
done
diff \$out/$a.symbols \$out/$b.symbols > \$out/symbols.diff || true
echo \"-- $a vs $b\"
echo \"only in $a: \$(grep -c '^<' \$out/symbols.diff || true)\"
echo \"only in $b: \$(grep -c '^>' \$out/symbols.diff || true)\"
for n in $a $b; do echo \"\$n size (text data bss): \$(awk '{print \$1, \$2, \$3}' \$out/\$n.size)\"; done
echo \"full diff: \$out/symbols.diff\"
"
