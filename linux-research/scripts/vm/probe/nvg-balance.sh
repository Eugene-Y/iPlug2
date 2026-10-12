#!/usr/bin/env bash
# NanoVG GL object balance over editor open/close cycles in REAPER: counts texture/context creates and deletes
# (gdb breakpoints in the plug-in) and REAPER's RSS. Runs on the VM via vm-run.sh (any X display works).
# Usage: nvg-balance.sh <out> "<FMT: Name>" <plugin> [cycles=10]
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; N="${4:-10}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
rpr() { "$HERE/rpr.sh" "$1"; }
rss() { awk '/VmRSS/{print $2}' /proc/$RPID/status; }
rpr "reaper.InsertTrackAtIndex(0,true); out(reaper.TrackFX_AddByName(reaper.GetTrack(0,0),\"$FX\",false,-1))" >/dev/null
# one open/close first, so one-time caches (shader cache, fonts) are filled before counting
rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,3)" >/dev/null; sleep 2
rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,2)" >/dev/null; sleep 1
R0=$(rss)
timeout -s INT $((N * 5 + 40)) gdb -q -batch -p "$RPID" -x "$(dirname "$0")/nvg-tex.gdb" </dev/null 2>&1 \
  | grep --line-buffered -E '^ *[0-9]+\.[0-9]+ ' > "$OUT/nvg-events.txt" &
for i in $(seq 60); do grep -q attached "$OUT/nvg-events.txt" 2>/dev/null && break; sleep 0.5; done
for i in $(seq "$N"); do
  echo "$(date +%s.%N | cut -c1-14) cycle $i" >> "$OUT/cycles.txt"
  rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,3)" >/dev/null; sleep 1.5
  rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,2)" >/dev/null; sleep 1
done
R1=$(rss)
sleep 3; pkill -INT -f "gdb -q -batch -p $RPID"; wait
{ echo "cycles=$N rss_kb before=$R0 after=$R1 per_cycle_kb=$(( (R1 - R0) / N ))"; awk '{print $2}' "$OUT/nvg-events.txt" | sort | uniq -c; awk '$2=="create-tex"{print $3,$4}' "$OUT/nvg-events.txt" | sort | uniq -c; } | tee "$OUT/RESULT.txt"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
