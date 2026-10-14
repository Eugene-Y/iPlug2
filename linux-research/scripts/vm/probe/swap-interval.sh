#!/usr/bin/env bash
# Which swap-control call the editor makes on this GL driver, and the swap interval in effect while it draws.
# Usage: swap-interval.sh <out> "<FMT: Name>" <plugin>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
glxinfo 2>/dev/null | grep -E "OpenGL renderer|^GLX version" > "$OUT/glx.txt"
glxinfo 2>/dev/null | sed -n '/^GLX extensions:/,/^[A-Z]/p' | tr -s ' ,' '\n' | grep -i swap | sed 's/^/effective GLX ext: /' >> "$OUT/glx.txt"
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); out(reaper.TrackFX_AddByName(reaper.GetTrack(0,0),\"$FX\",false,-1))" >/dev/null
timeout -s INT 40 gdb -q -batch -p "$RPID" -x "$(dirname "$0")/swap-interval.gdb" </dev/null > "$OUT/gdb.txt" 2>&1 &
sleep 12
"$HERE/rpr.sh" "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,3)" >/dev/null
sleep 15; pkill -INT -f "gdb -q -batch -p $RPID"; wait
{ cat "$OUT/glx.txt"; grep -E '^(HIT|AT)' "$OUT/gdb.txt"; } | tee "$OUT/RESULT.txt"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
