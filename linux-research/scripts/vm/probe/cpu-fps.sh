#!/usr/bin/env bash
# CPU (pidstat) of REAPER, Xorg and gnome-shell, and editor frames/s (gdb count of IGraphics::Draw), with the editor
# open and idle, and while a knob is dragged continuously.
# Usage: cpu-fps.sh <out> "<FMT: Name>" <plugin> <knobX> <knobY>     (knob position relative to the editor view)
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; KX="$4"; KY="$5"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
XPID=$(pgrep -x Xorg | head -1); SPID=$(pgrep -x gnome-shell | head -1)
cpu() { # <label> <secs>: average %CPU per process
  pidstat -u -p "$RPID,$XPID,$SPID" 1 "$2" | awk -v l="$1" '/^Average/ && $NF!="Command" {printf "%s cpu %s=%s%%\n", l, $NF, $(NF-2)}' | tee -a "$R"
}
fps() { # <label> <secs>: Draw calls per second over <secs>, timed from the first to the last hit
  timeout -s INT $(( $2 + 20 )) gdb -q -batch -p "$RPID" -x "$(dirname "$0")/draw-times.gdb" </dev/null 2>/dev/null \
    | grep --line-buffered -E '^(DRAW|ATTACHED)' > "$OUT/draws-$1.txt" &
  for i in $(seq 80); do grep -q ATTACHED "$OUT/draws-$1.txt" 2>/dev/null && break; sleep 0.25; done
  sleep "$2"; pkill -INT -f "gdb -q -batch -p $RPID"; wait $!
  awk -v l="$1" '/^DRAW/{n++; if(!f) f=$2; t=$2} END{d=t-f; printf "%s draws=%d over %.1f s = %.1f/s\n", l, n, d, (d>0 ? (n-1)/d : 0)}' "$OUT/draws-$1.txt" | tee -a "$R"
}
view_origin() {
  local w; w=$(xdotool search --onlyvisible --name "$NAME.*Track 1" | tail -1)
  xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }'
}
drag_loop() { # <secs>: hold the knob and move up and down
  read -r OX OY < <(view_origin)
  xdotool mousemove $((OX + KX)) $((OY + KY)) mousedown 1
  local end=$(( $(date +%s) + $1 )) i=0
  while [ "$(date +%s)" -lt "$end" ]; do xdotool mousemove $((OX + KX)) $((OY + KY + (i % 40) - 20)); i=$((i + 1)); sleep 0.02; done
  xdotool mouseup 1
}
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); out(reaper.TrackFX_AddByName(reaper.GetTrack(0,0),\"$FX\",false,-1))" >/dev/null
sleep 2; cpu "closed" 10
"$HERE/rpr.sh" "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,3)" >/dev/null; sleep 3
xdotool mousemove 5 400
cpu "open-idle" 10
fps "open-idle" 10
( drag_loop 14 ) & sleep 5; import -window root "$OUT/mid-drag.png"; cpu "dragging" 8; wait
( drag_loop 30 ) & DL=$!; fps "dragging" 10; wait $DL
import -window root "$OUT/after-drag.png"
echo "overview active at end: $(gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell --method org.freedesktop.DBus.Properties.Get org.gnome.Shell OverviewActive)" | tee -a "$R"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
