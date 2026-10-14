#!/usr/bin/env bash
# P3b's delayed focus take under the session's window manager, repeated: N times double-click IPlugControls'
# IVNumberBox, type, press Escape. The edit must hold the X focus (Escape reaches the edit: value unchanged, REAPER's
# FX window stays). Records the X focus trail of every trial (focuswatch) and, with an IGRAPHICS_TRACE_LINUX_INPUT
# build, the editor's own focus log.
# Usage: focus-edit.sh <out> "<FMT: IPlugControls>" <plugin> [trials=10] [hold=0.08]
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; N="${4:-10}"; HOLD="${5:-0.08}"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
gcc -O1 -include stdlib.h -o /tmp/focuswatch "$HERE/probe/focuswatch.c" -lX11 2>/dev/null || { log "cannot build focuswatch"; exit 1; }
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
rpr() { "$HERE/rpr.sh" "$1"; }
show() { rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 2; }
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1)" >/dev/null
show
# move the FX window right, so REAPER's track panel (left) can be clicked between trials
fxwin0() { for w in $(xdotool search --onlyvisible --name "$NAME.*Track 1" 2>/dev/null); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1; }
wmctrl -i -r "$(fxwin0)" -e 0,420,260,-1,-1; sleep 1
fxwin() { for w in $(xdotool search --onlyvisible --name "$NAME.*Track 1" 2>/dev/null); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1; }
origin() {
  local w; w=$(fxwin); [ -n "$w" ] || return 1
  xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }'
}
gain() { rpr "local ok,s=reaper.TrackFX_GetFormattedParamValue(reaper.GetTrack(0,0),0,0,''); out(s)"; }
log "window manager: $(wmctrl -m | awk -F': ' '/^Name/{print $2}'), double-click hold $HOLD s"
ok=0
for i in $(seq "$N"); do
  read -r OX OY < <(origin) || { log "trial $i: FX window missing, reopening"; show; read -r OX OY < <(origin); }
  # click REAPER's main window first, so every trial starts with the FX window unfocused
  main=$(for w in $(xdotool search --onlyvisible --name '^\[unsaved project\] - REAPER'); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | head -1)
  eval "$(xdotool getwindowgeometry --shell "$main")"; xdotool mousemove $((X + 150)) $((Y + 300)) click 1; sleep 0.5
  before=$(gain)
  timeout 3 /tmp/focuswatch 2.5 > "$OUT/focus-$i.txt" 2>&1 &
  FW=$!
  xdotool mousemove $((OX + 941)) $((OY + 437))
  for k in 1 2; do xdotool mousedown 1; sleep "$HOLD"; xdotool mouseup 1; sleep 0.08; done
  sleep 0.8
  xdotool type --delay 40 13; xdotool key Escape; sleep 0.7
  wait "$FW"
  after=$(gain); w=$(fxwin)
  if [ -n "$w" ] && [ "$after" = "$before" ]; then ok=$((ok + 1)); res=ok; else res="FAIL (window ${w:-gone}, gain '$before' -> '$after')"; fi
  log "trial $i: $res; focus trail: $(awk '{print $3}' "$OUT/focus-$i.txt" | tr '\n' ' ')"
  if [ -z "$w" ]; then show; wmctrl -i -r "$(fxwin)" -e 0,420,260,-1,-1; sleep 1; fi
done
log "RESULT: $ok/$N edits kept the focus until Escape"
grep -E 'focus|Focus' /tmp/reaper.log > "$OUT/editor-focus-trace.txt" 2>/dev/null
log "trace lines: $(wc -l < "$OUT/editor-focus-trace.txt")"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
