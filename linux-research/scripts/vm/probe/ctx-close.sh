#!/usr/bin/env bash
# REAPER's VST3 parameter context menu runs a nested loop inside our OnMouseDown (see ctx-menu.sh). Does a host
# action during that loop reach us, and does it survive: close the FX window, or delete the FX, while the menu is open,
# then dismiss the menu. Crash backtraces via gdb-catch.sh.
# Usage: ctx-close.sh <out> "<VST3: IPlugControls>" <plugin> <close|delete>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; MODE="${4:-close}"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
rpr() { timeout 15 "$HERE/rpr.sh" "$1"; }
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 2.5
( "$HERE/gdb-catch.sh" "$RPID" "$OUT/gdb-crash.txt" & ); sleep 4
w=$(for x in $(xdotool search --onlyvisible --name "$NAME.*Track 1"); do xprop -id "$x" WM_CLASS | grep -q '"REAPER"' && echo "$x"; done | tail -1)
read -r OX OY < <(xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
xdotool mousemove $((OX + 322)) $((OY + 237)) mousedown 3; sleep 0.15; xdotool mouseup 3; sleep 1.5
log "menu windows: $(xdotool search --onlyvisible --name '^menu$' | wc -l)"
case "$MODE" in
  close)  r=$(rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,2); out('closed')");;
  delete) r=$(rpr "out(reaper.TrackFX_Delete(reaper.GetTrack(0,0),0))");;
esac
log "host action '$MODE' during the menu: '${r:-no answer within 15 s}'"
import -window root "$OUT/x01-after-action.png"
log "FX window present: $(xdotool search --onlyvisible --name "$NAME.*Track 1" | wc -l)"
xdotool key Escape; sleep 2
log "after Escape: menu windows $(xdotool search --onlyvisible --name '^menu$' | wc -l), FX window $(xdotool search --onlyvisible --name "$NAME.*Track 1" | wc -l)"
r2=$(rpr "local t=reaper.GetTrack(0,0); out(reaper.TrackFX_GetCount(t))")
log "FX count afterwards: '${r2:-no answer}'"
import -window root "$OUT/x02-end.png"
if ps -o stat= -p "$RPID" | grep -qv Z; then log "REAPER alive"; else log "REAPER DEAD"; fi
sleep 1; grep -E "^(Thread|#|Program|\[Inferior)" "$OUT/gdb-crash.txt" | head -20 >> "$R"
pkill -INT -f "^sudo -n /usr/bin/gdb -q -batch -p $RPID" 2>/dev/null
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
