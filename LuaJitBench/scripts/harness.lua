-- Called by src/runner.c in each new Lua state: sets the JIT mode and counts the trace events.
local jit = require("jit")
local util = require("jit.util")
local vmdef = require("jit.vmdef")

local M = {}
local stats

-- Same text as the abort lines of jit/v.lua.
local function abort_text(code, info)
  if type(code) ~= "number" then return tostring(code) end
  local fmt = vmdef.traceerr[code]
  if not fmt then return "error " .. code end
  if type(info) == "function" then
    info = "function"
  elseif fmt == "NYI: bytecode %s" then
    info = vmdef.bcnames:sub(6 * info + 1, 6 * info + 6):gsub(" +$", "")
  end
  local ok, text = pcall(string.format, fmt, info)
  return ok and text or fmt
end

local function on_trace(what, tr, _func, _pc, otr, oex)
  if what == "start" then
    stats.starts = stats.starts + 1
  elseif what == "stop" then
    stats.stops = stats.stops + 1
    local mcode = util.tracemc(tr)
    if mcode then stats.mcode_bytes = stats.mcode_bytes + #mcode end
  elseif what == "abort" then
    stats.aborts = stats.aborts + 1
    local text = abort_text(otr, oex)
    stats.abort_counts[text] = (stats.abort_counts[text] or 0) + 1
  end
end

-- Returns the run function and the n of the bench.
function M.prepare(name, mode)
  stats = { starts = 0, stops = 0, aborts = 0, mcode_bytes = 0, abort_counts = {} }
  if mode == "jit" then
    jit.on()
    jit.flush()
  else
    jit.off()
  end
  local bench = require("bench." .. name)
  jit.attach(on_trace, "trace")
  return bench.run, bench.n
end

-- Returns starts, stops, aborts, the most frequent abort text, and the code bytes.
function M.finish()
  jit.attach(on_trace)
  local top, top_count = "", 0
  for text, count in pairs(stats.abort_counts) do
    if count > top_count or (count == top_count and text < top) then
      top, top_count = text, count
    end
  end
  return stats.starts, stats.stops, stats.aborts, top, stats.mcode_bytes
end

-- The VM states of jit.profile.
local VMSTATE_NAMES = { N = "compiled", I = "interpreted", C = "C code", G = "GC", J = "JIT compiler" }
local VMSTATE_ORDER = { "N", "I", "C", "G", "J" }
local PROFILE_TOP = 10

local function sorted_by_count(counts)
  local list = {}
  for key, count in pairs(counts) do list[#list + 1] = { key = key, count = count } end
  table.sort(list, function(a, b)
    if a.count ~= b.count then return a.count > b.count end
    return a.key < b.key
  end)
  return list
end

local function percent(count, total)
  return string.format("%.1f%%", 100 * count / total)
end

-- Runs the bench again and again for about `seconds`, with the JIT on, and samples it each 1 ms.
-- Returns the result as lines of text.
function M.profile(name, seconds)
  local profile = require("jit.profile")
  local now_ms = require("ljb").now_ms
  jit.on()
  jit.flush()
  local bench = require("bench." .. name)

  local where_counts, state_counts, total = {}, {}, 0
  profile.start("fi1", function(thread, samples, vmstate)
    local where = profile.dumpstack(thread, "F", 1)
    where_counts[where] = (where_counts[where] or 0) + samples
    state_counts[vmstate] = (state_counts[vmstate] or 0) + samples
    total = total + samples
  end)
  local start, runs = now_ms(), 0
  repeat
    bench.run(bench.n)
    runs = runs + 1
  until now_ms() - start >= seconds * 1000
  profile.stop()

  local lines = { string.format("profile %s: %d runs in %.0f ms, %d samples", name, runs, now_ms() - start, total) }
  if total == 0 then return table.concat(lines, "\n") end

  local states = {}
  for _, key in ipairs(VMSTATE_ORDER) do
    states[#states + 1] = VMSTATE_NAMES[key] .. " " .. percent(state_counts[key] or 0, total)
  end
  lines[#lines + 1] = "profile vm states: " .. table.concat(states, ", ")
  for i, entry in ipairs(sorted_by_count(where_counts)) do
    if i > PROFILE_TOP then break end
    lines[#lines + 1] = string.format("profile top %d: %s %s", i, percent(entry.count, total), entry.key)
  end
  return table.concat(lines, "\n")
end

return M
