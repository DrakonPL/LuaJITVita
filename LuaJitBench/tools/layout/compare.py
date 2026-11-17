"""Compares the struct offsets of the host build of buildvm with the Vita build.

Usage: compare.py <host.txt> <target.s>
host.txt: "name value" lines from the host program.
target.s: the Vita assembly of layout.c. It holds layout[] as ".word <name label>", ".word <value>" pairs.
Exit code 1 if a value differs.
"""
import re
import sys

host = [line.rsplit(" ", 1) for line in open(sys.argv[1]).read().splitlines()]
asm = open(sys.argv[2]).read()
body = asm[asm.index("layout:"):]
words = re.findall(r"\.word\s+(\S+)", body)[: 2 * len(host)]
target_values = [int(w, 0) for w in words[1::2]]
if len(target_values) != len(host):
    sys.exit("cannot read %d values from %s" % (len(host), sys.argv[2]))

diffs = 0
for (name, host_value), target_value in zip(host, target_values):
    differs = int(host_value) != target_value
    diffs += differs
    print("%-22s host %7d  vita %7d%s" % (name, int(host_value), target_value, "  <-- DIFF" if differs else ""))
print("%d values, %d differ" % (len(host), diffs))
sys.exit(1 if diffs else 0)
