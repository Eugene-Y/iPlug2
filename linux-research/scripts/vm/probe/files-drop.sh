#!/usr/bin/env bash
# A real drag from GNOME Files (Nautilus, a GTK 4 source) into REAPER: one file from a fresh folder, dragged with the
# mouse to root position x,y over the plug-in's FX window (moved to 430,120), then what REAPER shows.
# Usage: files-drop.sh <out> "<FMT: Name>" <plugin> <x,y> [via-y] [file name=layout.hvoya] [file to copy]
# Files sits at 20,80 400x500, its first icon at 178,255. via-y: go to that height first, then straight across.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; TARGET="$4"; VIA_Y="${5:-}"; NAME="${6:-layout.hvoya}"; SRC="${7:-}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
DIR=$(mktemp -d ~/files-drop-XXXX)
if [ -n "$SRC" ]; then cp "$SRC" "$DIR/$NAME"; else printf '# hvoya v1\n# plugin Gneiss 1.0.0\n\n[user-tab]\nslot 0 1\nslot 2\n' > "$DIR/$NAME"; fi
"$HERE/reaper-up.sh" "$PLUG" >/dev/null
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
W=$(for w in $(xdotool search --onlyvisible --name "${FX#*: }.*Track 1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
[ -n "$W" ] && wmctrl -i -r "$W" -e 0,430,120,-1,-1; sleep 1
log "FX window $W: $(xprop -id "$W" XdndProxy 2>&1)"
pkill -x nautilus; sleep 0.5
GDK_DEBUG=dnd setsid nautilus --new-window "$DIR" > "$OUT/nautilus-dnd.txt" 2>&1 < /dev/null &
sleep 4
N=$(xdotool search --onlyvisible --class org.gnome.Nautilus | tail -1)
wmctrl -i -r "$N" -e 0,20,80,400,500; wmctrl -i -a "$N"; sleep 1
import -window root "$OUT/before.png"
TX=${TARGET%,*}; TY=${TARGET#*,}
SY=255; xdotool mousemove 178 255 sleep 0.3 mousedown 1 sleep 0.2
if [ -n "$VIA_Y" ]; then xdotool mousemove 190 "$VIA_Y" sleep 0.1; SY=$VIA_Y; TY=$VIA_Y; fi
for i in 1 2 3 4 5 6 7 8 9 10; do xdotool mousemove $((178 + (TX - 178) * i / 10)) $((SY + (TY - SY) * i / 10)) sleep 0.08; done
sleep 0.5; xdotool mousemove $((TX + 2)) $((TY + 1)) sleep 0.4 mouseup 1
sleep 3
import -window root "$OUT/after.png"
log "REAPER title: $(xdotool search --onlyvisible --name 'REAPER v' getwindowname %@ 2>/dev/null | head -1)"
log "tracks: $("$HERE/rpr.sh" 'out(reaper.CountTracks(0))')"
log "FX window still there: $(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" >/dev/null 2>&1 && echo yes || echo no)"
log "REAPER alive: $(pgrep -x reaper >/dev/null && echo yes || echo no)"
grep "iPlug2 drop" /tmp/reaper.log > "$OUT/drop-trace.txt" 2>/dev/null
pkill -x nautilus; rm -rf "$DIR"
