"""Makes scripts/bench/std/*.lua from LuaJIT-test-cleanup/bench. Edit this file, not the output.
Usage: python tools/make_std_benches.py <LuaJIT-test-cleanup>/bench scripts/bench/std

Each module keeps the original code up to the driver (arg parsing and io.write) unchanged,
then adds a run(n) driver that returns a checksum.
"""
import os
import sys

SRC = sys.argv[1]  # LuaJIT-test-cleanup/bench
DST = sys.argv[2]  # LuaJitBench/scripts/bench/std

HEADER = "-- From LuaJIT-test-cleanup/bench/{src} (see README.md). Only the driver at the end is new.\n"

BENCHES = [
    {
        "name": "fib", "src": "recursive-fib.lua", "cut": "local n = tonumber(arg[1]) or 10", "n": 31,
        "tail": """local function run(n)
  return fib(n)
end
""",
    },
    {
        "name": "nbody", "src": "nbody.lua", "cut": "local N = tonumber(arg and arg[1]) or 1000", "n": 50000,
        "tail": """-- The checksum is the sum of the two energies that the original prints.
local function run(N)
  local nbody = #bodies
  offsetMomentum(bodies, nbody)
  local e0 = energy(bodies, nbody)
  for i=1,N do advance(bodies, nbody, 0.01) end
  return string.format("%0.6f", e0 + energy(bodies, nbody))
end
""",
    },
    {
        "name": "spectral_norm", "src": "spectral-norm.lua", "cut": "local N = tonumber(arg and arg[1]) or 100", "n": 220,
        "tail": """local function run(N)
  local u, v, t = {}, {}, {}
  for i=1,N do u[i] = 1 end

  for i=1,10 do AtAv(u, v, t, N) AtAv(v, u, t, N) end

  local vBv, vv = 0, 0
  for i=1,N do
    local ui, vi = u[i], v[i]
    vBv = vBv + ui*vi
    vv = vv + vi*vi
  end
  return string.format("%0.6f", math.sqrt(vBv / vv))
end
""",
    },
    {
        "name": "fannkuch", "src": "fannkuch.lua", "cut": "local n = tonumber(arg and arg[1]) or 1", "n": 9,
        "prefix": "-- The algorithm writes its first 30 permutations. This stub drops them.\nlocal io = { write = function() end }\n",
        "tail": """local function run(n)
  return fannkuch(n)
end
""",
    },
    {
        "name": "binary_trees", "src": "binary-trees.lua", "cut": "local N = tonumber(arg and arg[1]) or 0", "n": 11,
        "tail": """-- The checksum is the sum of the check values that the original prints.
local function run(N)
  local mindepth = 4
  local maxdepth = mindepth + 2
  if maxdepth < N then maxdepth = N end
  local sum = 0

  do
    local stretchdepth = maxdepth + 1
    local stretchtree = BottomUpTree(0, stretchdepth)
    sum = sum + ItemCheck(stretchtree)
  end

  local longlivedtree = BottomUpTree(0, maxdepth)

  for depth=mindepth,maxdepth,2 do
    local iterations = 2 ^ (maxdepth - depth + mindepth)
    local check = 0
    for i=1,iterations do
      check = check + ItemCheck(BottomUpTree(1, depth)) +
              ItemCheck(BottomUpTree(-1, depth))
    end
    sum = sum + check
  end

  return sum + ItemCheck(longlivedtree)
end
""",
    },
    {
        "name": "ray", "src": "ray.lua", "cut": "local level, n, ss = tonumber(arg[1]) or 9, tonumber(arg[2]) or 256, 4", "n": 32,
        "tail": """-- n is the image size. The level is 4, as in PARAM_arm.txt. The checksum is the sum of the pixels.
local function run(n)
  local level, ss = 4, 4
  local iss = 1/ss
  local gf = 255/(ss*ss)
  local light = { unitise(-1, -3, 2) }
  ilight = { -light[1], -light[2], -light[3] }
  local camera = { 0, 0, -4 }
  local dir = { 0, 0, 0 }

  local scene = create(level, {0, -1, 0}, 1)

  local sum = 0
  for y = n/2-1, -n/2, -1 do
    for x = -n/2, n/2-1 do
      local g = 0
      for d = y, y+.99, iss do
        for e = x, x+.99, iss do
          dir[1], dir[2], dir[3] = unitise(e, d, n)
          g = g + ray_trace(light, camera, dir, scene)
        end
      end
      sum = sum + math.floor(0.5 + g*gf)
    end
  end
  return sum
end
""",
    },
]

# The whole original program is the driver loop. The loop is the same. The write of each row becomes a sum.
MANDELBROT = """-- From LuaJIT-test-cleanup/bench/mandelbrot.lua (see README.md).
-- The loop is the original. In place of the write of each row, the checksum adds the row bytes.
local function run(N)
  local M, ba, bb, buf = 2/N, 2^(N%8+1)-1, 2^(8-N%8), {}
  local sum = 0
  for y=0,N-1 do
    local Ci, b, p = y*M-1, 1, 0
    for x=0,N-1 do
      local Cr = x*M-1.5
      local Zr, Zi, Zrq, Ziq = Cr, Ci, Cr*Cr, Ci*Ci
      b = b + b
      for i=1,49 do
        Zi = Zr*Zi*2 + Ci
        Zr = Zrq-Ziq + Cr
        Ziq = Zi*Zi
        Zrq = Zr*Zr
        if Zrq+Ziq > 4.0 then b = b + 1; break; end
      end
      if b >= 256 then p = p + 1; buf[p] = 511 - b; b = 1; end
    end
    if b ~= 1 then p = p + 1; buf[p] = (ba-b)*bb; end
    for i=1,p do sum = sum + buf[i] end
  end
  return sum
end

return { name = "mandelbrot", run = run, n = 360 }
"""


def main():
    os.makedirs(DST, exist_ok=True)
    for bench in BENCHES:
        original = open(os.path.join(SRC, bench["src"]), encoding="utf-8").read().replace("\r\n", "\n")
        cut = original.index(bench["cut"])
        assert original.count(bench["cut"]) == 1, bench["name"]
        text = (HEADER.format(src=bench["src"]) + bench.get("prefix", "") + original[:cut].rstrip("\n") + "\n\n"
                + bench["tail"] + '\nreturn {{ name = "{0}", run = run, n = {1} }}\n'.format(bench["name"], bench["n"]))
        open(os.path.join(DST, bench["name"] + ".lua"), "w", encoding="utf-8", newline="\n").write(text)
        print("wrote", bench["name"])
    open(os.path.join(DST, "mandelbrot.lua"), "w", encoding="utf-8", newline="\n").write(MANDELBROT)
    print("wrote mandelbrot")


main()
