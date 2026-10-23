-- Run inside Ardour's GUI (Window > Scripting): dofile("<path>/gui-probe.lua"); probe(<mode>)
-- Appends to ~/t013-ardour-probe.txt: per plug-in insert, latency as Ardour sees it, a few parameter
-- names/values; mode "os" sets Gneiss' oversampling to its top choice (8x), "os1" back to 1x. `only` limits the
-- dump to one track. Ardour puts Bypass at index 0, so plug-in parameter i is Ardour index i + 1; values are plain.
local GNEISS_OVERSAMPLING = 29
local GNEISS_OS_TOP = 3
local function try(o, m) local ok, v = pcall(function() return o[m](o) end); return ok and v or "n/a" end
function probe(mode, only)
  local f = io.open(os.getenv("HOME") .. "/t013-ardour-probe.txt", "a")
  local function out(...) local t = {...}; for i = 1, #t do t[i] = tostring(t[i]) end; f:write(table.concat(t, "\t"), "\n") end
  out("== " .. tostring(mode) .. " " .. os.date("%T"))
  for r in Session:get_routes():iter() do
    local proc = r:nth_plugin(0)
    if not proc:isnil() and (only == nil or r:name() == only) then
      local pi = proc:to_insert()
      local p = pi:plugin(0)
      out(r:name(), p:name(), "params", p:parameter_count(), "plugin.signal_latency", try(p, "signal_latency"),
          "insert.effective_latency", try(pi, "effective_latency"), "insert.signal_latency", try(pi, "signal_latency"),
          "route.signal_latency", try(r, "signal_latency"), "route.playback_latency", try(r, "playback_latency"))
      for _, j in ipairs({0, 1, 2, GNEISS_OVERSAMPLING}) do
        if j < p:parameter_count() then
          local v = ARDOUR.LuaAPI.get_plugin_insert_param(pi, j)
          out("", j, p:parameter_label(j), v)
        end
      end
      if r:name() == "Gneiss" and (mode == "os" or mode == "os1") then
        out("set", GNEISS_OVERSAMPLING, mode == "os" and GNEISS_OS_TOP or 0,
            ARDOUR.LuaAPI.set_plugin_insert_param(pi, GNEISS_OVERSAMPLING, mode == "os" and GNEISS_OS_TOP or 0))
      end
    end
  end
  f:close()
end
