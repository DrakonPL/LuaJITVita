-- From LuaJIT-test-cleanup/bench/binary-trees.lua (see README.md). Only the driver at the end is new.

local function BottomUpTree(item, depth)
  if depth > 0 then
    local i = item + item
    depth = depth - 1
    local left, right = BottomUpTree(i-1, depth), BottomUpTree(i, depth)
    return { item, left, right }
  else
    return { item }
  end
end

local function ItemCheck(tree)
  if tree[2] then
    return tree[1] + ItemCheck(tree[2]) - ItemCheck(tree[3])
  else
    return tree[1]
  end
end

-- The checksum is the sum of the check values that the original prints.
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

return { name = "binary_trees", run = run, n = 11 }
