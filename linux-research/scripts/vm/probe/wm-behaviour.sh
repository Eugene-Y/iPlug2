#!/usr/bin/env bash
# The editor under a real window manager (GNOME Mutter, X11): stacking and focus when clicking between REAPER and the
# editor, moving the FX window, minimize/restore of the FX window and of REAPER's main window. Draw calls are counted
# while minimized (drawing is supposed to stop while our window is unmapped).
# Usage: wm-behaviour.sh <out> "<FMT: Name>" <plugin>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
# Mutter's frame windows (mutter-x11-frames) carry the title too: pick REAPER's own windows by class.
reaper_win() { for w in $(xdotool search --onlyvisible --name "$1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1; }
MAIN=$(reaper_win '^\[unsaved project\] - REAPER')
FXW=$(reaper_win "$NAME.*Track 1")
[ -n "$MAIN" ] && [ -n "$FXW" ] || { log "windows not found: main='$MAIN' fx='$FXW'"; exit 1; }
xwi() { [ -n "$2" ] && xwininfo "$@"; } # an empty -id would make xwininfo wait for a click on the screen
view() { # "ID X Y W H" of the largest window inside the FX window: the editor's own X window
  xwi -id "$FXW" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=$1" "a[2]" "a[3]" "g[1]" "g[2] } } END { print r }'
}
ours() { view | cut -d' ' -f1; }
state() { # <label>
  local a; a=$(xprop -root _NET_ACTIVE_WINDOW | awk '{print $NF}')
  local an; an=$(xdotool getwindowname "$a" 2>/dev/null)
  local top; top=$(xprop -root _NET_CLIENT_LIST_STACKING | tr ',' '\n' | tail -1 | tr -d ' ')
  local w; w=$(ours)
  log "$1: active='$an' top-of-stack='$(xdotool getwindowname "$top" 2>/dev/null)' fx-wm-state=$(xprop -id "$FXW" WM_STATE | awk '/state:/{print $3}') editor-map=$(xwi -id "$w" | awk '/Map State/{print $3}') view=$(view | cut -d' ' -f2-)"
}
shot() { import -window root "$OUT/$1.png"; }
blank() { # <png>: stddev of the editor area
  read -r _ X Y W H < <(view)
  convert "$OUT/$1.png" -crop "${W}x${H}+${X}+${Y}" +repage -colorspace Gray -format '%[fx:standard_deviation]' info:
}
draws() { # <label> <secs> [action...]: Draw calls while <action> runs (or idle)
  local lbl="$1" secs="$2"; shift 2
  timeout -s INT $((secs + 20)) gdb -q -batch -p "$RPID" -x "$(dirname "$0")/draw-times.gdb" </dev/null 2>/dev/null \
    | grep --line-buffered -E '^(DRAW|ATTACHED)' > "$OUT/draws-$lbl.txt" &
  for i in $(seq 80); do grep -q ATTACHED "$OUT/draws-$lbl.txt" 2>/dev/null && break; sleep 0.25; done
  [ $# -gt 0 ] && "$@"
  sleep "$secs"; pkill -INT -f "^sudo -n /usr/bin/gdb -q -batch -p $RPID" 2>/dev/null; wait $!
  log "$lbl: $(grep -c ^DRAW "$OUT/draws-$lbl.txt") Draw calls in ${secs} s"
}

abs_xy() { xwi -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
state "opened"; shot w01-opened
# programmatic move (the FX window covers most of REAPER's main window at first)
wmctrl -i -r "$FXW" -e 0,420,260,-1,-1; sleep 1.5
state "wmctrl move"; shot w02-wmctrl-moved; log "  editor stddev after wmctrl move: $(blank w02-wmctrl-moved)"
# interactive move: drag the title bar (Mutter's frame) by +120,+40
read -r CX CY < <(abs_xy "$FXW")
xdotool mousemove $((CX + 200)) $((CY - 16)) mousedown 1
for i in $(seq 1 12); do xdotool mousemove $((CX + 200 + i * 10)) $((CY - 16 + i * 3)); sleep 0.04; done
xdotool mouseup 1; sleep 1.5
state "title-bar drag"; shot w03-dragged; log "  editor stddev after title-bar drag: $(blank w03-dragged)"
# focus: click REAPER's track panel (left part of the main window, now uncovered), then into the editor
read -r MX MY < <(abs_xy "$MAIN")
xdotool mousemove $((MX + 150)) $((MY + 300)) click 1; sleep 1
state "clicked REAPER main"; shot w04-click-main
read -r _ VX VY VW VH < <(view)
xdotool mousemove $((VX + 60)) $((VY + 300)) click 1; sleep 1
state "clicked inside editor"; shot w05-click-editor
draws "after-move-idle" 3
# minimize / restore the FX window
xdotool windowminimize "$FXW"; sleep 1.5
state "fx minimized"; shot w06-fx-min
draws "fx-minimized" 4 xdotool mousemove 700 400
wmctrl -i -a "$FXW"; sleep 1.5
state "fx restored"; shot w07-fx-restored; log "  editor stddev after restore: $(blank w07-fx-restored)"
# minimize / restore REAPER's main window
xdotool windowminimize "$MAIN"; sleep 1.5
state "main minimized"; shot w08-main-min
draws "main-minimized" 4
wmctrl -i -a "$MAIN"; sleep 1.5; wmctrl -i -a "$FXW"; sleep 1.5
state "main+fx restored"; shot w09-main-restored; log "  editor stddev after main restore: $(blank w09-main-restored)"
ps -o stat= -p "$RPID" >/dev/null && log "REAPER alive" || log "REAPER DEAD"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
