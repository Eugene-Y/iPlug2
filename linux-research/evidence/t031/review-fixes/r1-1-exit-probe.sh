#!/usr/bin/env bash
# R1-1 re-check: at REAPER exit, ~SWELLHostLoopBackend must find its backend list intact (never freed)
set -uo pipefail
HERE=/Volumes/Storage/code/IPlug2-T-031/linux-research/scripts
OUT=/Volumes/Storage/code/IPlug2-T-031/build/t031/gdbexit
REAPER_EXIT_FILE=/tmp/reaper.exit "$HERE/reaper-up.sh" /Volumes/Storage/code/IPlug2-T-031/build/t031/out/reaper_IPlugReaperExtension.so > "$OUT/up.txt"
RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" 'reaper.Main_OnCommand(reaper.NamedCommandLookup("_IPlugReaperExtension: Action 3 - Show/Hide UI"),0)' >/dev/null
sleep 2
SB=_ZZN5iplug20SWELLHostLoopBackend8BackendsEvE9sBackends
cat > /tmp/gx.cmds <<G
set pagination off
set breakpoint pending on
set \$l = *(void**)&$SB
set \$b = *(void**)\$l
printf "list=%p buf=%p first=%p\n", \$l, \$b, *(void**)\$b
break free if \$x0 == \$b || \$x0 == \$l
commands
printf "=== free of the backend list or its buffer\n"
bt 6
continue
end
break iplug::SWELLHostLoopBackend::~SWELLHostLoopBackend
commands
printf "=== ~SWELLHostLoopBackend this=%p list first=%p\n", \$x0, *(void**)(*(void**)\$l)
bt 6
continue
end
continue
G
gdb -q -batch -p "$RPID" -x /tmp/gx.cmds > "$OUT/gdb.txt" 2>&1 &
G=$!
sleep 4
"$HERE/rpr.sh" 'reaper.Main_OnCommand(40004, 0); out("quit")' >/dev/null
for i in $(seq 60); do kill -0 $G 2>/dev/null || break; sleep 0.5; done
kill -INT $G 2>/dev/null; sleep 1; kill -9 $G 2>/dev/null
echo "exit: $(cat /tmp/reaper.exit 2>/dev/null)"
grep -E "^list=|^===|exited" "$OUT/gdb.txt"
