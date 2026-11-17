-- A number loop. On the host, the JIT compiles it into one trace.
local function run(n)
  local sum = 0
  for i = 1, n do
    sum = (sum + i * 3) % 999983
  end
  return sum
end

return { name = "trivial", run = run, n = 1000000 }
