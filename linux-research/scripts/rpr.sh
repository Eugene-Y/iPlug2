#!/usr/bin/env bash
# Run Lua inside the already running REAPER (reaper -nonewinst) and print what it writes to OUT.
# The Lua code may call out(str) to report. Runs INSIDE the GUI container.
# Usage: rpr.sh '<lua code>'
#   e.g. rpr.sh 'local t=reaper.GetTrack(0,0); out(reaper.TrackFX_GetCount(t))'
set -euo pipefail
export HOME=/tmp/rhome
LUA=$(mktemp /tmp/rpr-XXXX.lua); RES="$LUA.out"
cat > "$LUA" <<EOF
local __f = io.open("$RES", "w")
function out(...) local t={} for i,v in ipairs({...}) do t[#t+1]=tostring(v) end __f:write(table.concat(t," ").."\n") __f:flush() end
local ok, err = pcall(function()
$1
end)
if not ok then out("LUA ERROR: "..tostring(err)) end
__f:close()
EOF
/opt/REAPER/reaper -nonewinst "$LUA" >/dev/null 2>&1 &
for i in $(seq 100); do [ -s "$RES" ] && break; sleep 0.1; done
sleep 0.2
cat "$RES" 2>/dev/null || echo "(no output from lua within 10s)"
