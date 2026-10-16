#!/usr/bin/env bash
# Does the editor stop drawing while the host's window is minimized or fully covered, and redraw when shown again?
# Counts IGraphics::Draw calls (gdb) for a few seconds per phase: visible, FX window minimized, restored, covered by
# another window, uncovered. IPlugControls' meters animate, so a visible editor draws all the time.
# GUI container or VM; Usage: hidden-draws.sh <out> "<FMT: Name>" <plugin> [secs=3]
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; SECS="${4:-3}"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
FXW=$(for w in $(xdotool search --onlyvisible --name "$NAME.*Track 1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
[ -n "$FXW" ] || { log "FX window not found"; exit 1; }
read -r SW SH < <(xdpyinfo | awk '/dimensions:/{split($2,a,"x"); print a[1], a[2]}')
draws() { # <label>
  timeout -s INT $((SECS + 30)) gdb -q -batch -p "$RPID" -x "$HERE/vm/probe/draw-times.gdb" </dev/null 2>/dev/null \
    | grep --line-buffered -E '^(DRAW|ATTACHED)' > "$OUT/draws-$1.txt" &
  local g=$!
  for i in $(seq 120); do grep -q ATTACHED "$OUT/draws-$1.txt" 2>/dev/null && break; sleep 0.25; done
  sleep "$SECS"
  pkill -INT -f "gdb -q -batch -p $RPID" 2>/dev/null; wait "$g" 2>/dev/null
  log "$1: $(grep -c ^DRAW "$OUT/draws-$1.txt") Draw calls in $SECS s (FX WM_STATE $(xprop -id "$FXW" WM_STATE | awk '/state:/{print $3}'))"
}
shot() { import -window root "$OUT/$1.png"; }
draws visible
xdotool windowminimize "$FXW"; sleep 1.5
draws fx-minimized
xdotool windowactivate "$FXW" 2>/dev/null || xdotool windowmap "$FXW"; sleep 1.5
shot h01-restored
draws restored
xclock -geometry "${SW}x${SH}+0+0" -update 1 & CLOCK=$!
sleep 2
draws covered
kill "$CLOCK"; sleep 1.5
shot h02-uncovered
draws uncovered
ps -o stat= -p "$RPID" >/dev/null && log "REAPER alive" || log "REAPER DEAD"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
true
