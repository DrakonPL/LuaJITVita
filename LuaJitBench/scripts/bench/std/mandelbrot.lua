-- From LuaJIT-test-cleanup/bench/mandelbrot.lua (see README.md).
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
