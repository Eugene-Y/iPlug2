#!/usr/bin/env bash
# REAPER's VST3 parameter context menu (IComponentHandler3::createContextMenu + IContextMenu::popup) on the desktop:
# does popup() block (nested loop), does the editor keep drawing meanwhile, does choosing an item work
# ("Show track envelope" -> an FX envelope exists). Usage: ctx-menu.sh <out> "<VST3: IPlugControls>" <plugin>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
rpr() { "$HERE/rpr.sh" "$1"; }
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 2.5
w=$(for x in $(xdotool search --onlyvisible --name "$NAME.*Track 1"); do xprop -id "$x" WM_CLASS | grep -q '"REAPER"' && echo "$x"; done | tail -1)
read -r OX OY < <(xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
timeout -s INT 60 gdb -q -batch -p "$RPID" -x "$(dirname "$0")/ctx-menu.gdb" </dev/null 2>/dev/null \
  | grep --line-buffered -E '^(CTX|DRAW|ATTACHED)' > "$OUT/gdb.txt" &
for i in $(seq 120); do grep -q ATTACHED "$OUT/gdb.txt" 2>/dev/null && break; sleep 0.25; done
log "envelope before: $(rpr 'out(reaper.GetFXEnvelope(reaper.GetTrack(0,0),0,0,false) and "yes" or "no")')"
T0=$(date +%s.%N)
# a held right press, as a person clicks: with xdotool's same-millisecond release GTK dismisses the menu at once
xdotool mousemove $((OX + 322)) $((OY + 237)) mousedown 3; sleep 0.15; xdotool mouseup 3; sleep 2
import -window root "$OUT/m01-menu-open.png"
log "menu windows: $(xdotool search --onlyvisible --name '^menu$' | wc -l)"
# "Show track envelope": the 3rd row of the menu, which opens with its top-left at the click point
xdotool mousemove $((OX + 322 + 60)) $((OY + 237 + 50)); sleep 0.5
import -window root "$OUT/m02-hover.png"
xdotool click 1; sleep 1.5
import -window root "$OUT/m03-chosen.png"
pkill -INT -f "^sudo -n /usr/bin/gdb -q -batch -p $RPID" 2>/dev/null; wait
log "envelope after: $(rpr 'out(reaper.GetFXEnvelope(reaper.GetTrack(0,0),0,0,false) and "yes" or "no")')"
grep CTX "$OUT/gdb.txt" | tee -a "$R"
ent=$(awk '/CTX enter/{print $3; exit}' "$OUT/gdb.txt")
if [ -n "$ent" ]; then
  log "Draw calls during the 2 s after the context menu was requested: $(awk -v e="$ent" '/^DRAW/ && $2>e && $2<e+2 {n++} END{print n+0}' "$OUT/gdb.txt")"
fi
ps -o stat= -p "$RPID" >/dev/null && log "REAPER alive" || log "REAPER DEAD"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
