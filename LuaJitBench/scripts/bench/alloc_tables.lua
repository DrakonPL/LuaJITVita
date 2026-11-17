-- Makes many small tables with 2 to 8 array fields and drops them. The allocator and the GC do most of the work.
-- A ring keeps each table alive for a short time, so the JIT cannot sink the allocation.
local RING_SIZE = 256

local function run(n)
  local ring, sum = {}, 0
  for i = 1, n do
    local t = { i, i }
    for k = 3, 2 + i % 7 do
      t[k] = k
    end
    ring[i % RING_SIZE + 1] = t
    sum = sum + #t
  end
  return sum
end

return { name = "alloc_tables", run = run, n = 400000 }
