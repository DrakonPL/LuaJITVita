-- Builds strings with .. and joins them with table.concat. The allocator, the string table, and the GC do most of the work.
local PARTS_PER_JOIN = 64

local function run(n)
  local parts, total = {}, 0
  for i = 1, n do
    parts[#parts + 1] = "item" .. i .. ":" .. (i % 97)
    if #parts == PARTS_PER_JOIN then
      total = total + #table.concat(parts, ",")
      parts = {}
    end
  end
  return total + #table.concat(parts, ",")
end

return { name = "alloc_strings", run = run, n = 300000 }
