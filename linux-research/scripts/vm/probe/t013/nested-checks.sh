#!/usr/bin/env bash
# Review re-checks in the nested Wayland session (nested-start.sh first), Sand CLAP in the harness REAPER:
#  1. crossings: Enter/Leave of xev's own window and of the plug-in's view, with the pointer path, on the outer display;
#  2. a text entry with the trace counted live and after gdb flushes REAPER's stdio.
# Usage: nested-checks.sh <Sand.clap> <outdir>   (through vm-x.sh; outdir relative to linux-research/)
P="$1"; OUT="$2"; D="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
gcc -o /tmp/crossing-watch "$D/crossing-watch.c" -lX11 || exit 1
. "$D/nested-env.sh" || exit 1
nested_overview_off >/dev/null
V=$(bash "$D/nested-open.sh" "$P" "CLAP: Sand" 200,60 | tee "$OUT/open.txt" | grep '^view')
read -r _ VX VY _ <<<"$V"
[ -n "$VX" ] || { echo "no view"; exit 1; }
outer() { DISPLAY="$OUTER_DISPLAY" XAUTHORITY="$OUTER_XAUTHORITY" "$@"; }
# Outer-display points: the nested output starts at the nested window's origin (nested-view.sh's offset).
read -r OX OY < <(sed -n 's/.*offset \([0-9]*\) \([0-9]*\)).*/\1 \2/p' "$OUT/open.txt")
move() { echo "$(date +%T.%N | cut -c1-12) pointer -> $1 ($2,$3)"; outer xdotool mousemove "$2" "$3"; sleep 1.5; }

# 1a. xev's own window (any X11 client), at nested 900,500.
xev -geometry 200x150+900+500 -event mouse > "$OUT/xev-raw.txt" 2>&1 &
XEV=$!
# xev sets no _NET_WM_PID; wmctrl lists the client, not mutter's frame of the same title. xwininfo -id "" would wait
# for a click.
for i in $(seq 20); do XW=$(wmctrl -l | awk '/ Event Tester$/{print $1}' | head -1); [ -n "$XW" ] && break; sleep 0.5; done
[ -n "$XW" ] || { echo "no xev window"; kill "$XEV"; exit 1; }
wmctrl -i -r "$XW" -e 0,900,500,-1,-1; sleep 1
xwininfo -id "$XW" | grep -E "Absolute|Width|Height" > "$OUT/xev-window.txt"
{
  move "xev window" $((OX+1000)) $((OY+575))
  move "nested desktop (no X window)" $((OX+1250)) $((OY+740))
  move "xev window" $((OX+1000)) $((OY+575))
  move "outer desktop (out of the nested compositor)" 30 840
  move "xev window" $((OX+1000)) $((OY+575))
  move "REAPER main window (X)" $((OX+600)) $((OY+500))
} > "$OUT/xev-path.txt"
kill "$XEV"; wait "$XEV" 2>/dev/null
grep -E "^(EnterNotify|LeaveNotify)" "$OUT/xev-raw.txt" > "$OUT/xev-crossings.txt"

# 1b. The plug-in's view, with Sand's tooltip: hover comp, then leave to the nested desktop.
VIEWWIN=$(xdotool search --onlyvisible --name "Sand.*Track 1" | while read -r w; do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
VIEWWIN=$(xwininfo -id "$VIEWWIN" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); if (g[1]*g[2] > b) { b=g[1]*g[2]; r=$1 } } END { print r }')
/tmp/crossing-watch "$VIEWWIN" 20 > "$OUT/view-crossings.txt" &
CW=$!; sleep 1
{
  echo "view window $VIEWWIN at outer $VX,$VY"
  move "Sand comp knob (tooltip)" $((VX+256)) $((VY+94)); sleep 1
  DISPLAY="$OUTER_DISPLAY" XAUTHORITY="$OUTER_XAUTHORITY" import -window root "$OUT/tooltip-over-comp.png"
  move "nested desktop (no X window)" $((OX+1250)) $((OY+740))
  DISPLAY="$OUTER_DISPLAY" XAUTHORITY="$OUTER_XAUTHORITY" import -window root "$OUT/tooltip-after-leave-to-desktop.png"
  move "Sand comp knob (tooltip)" $((VX+256)) $((VY+94)); sleep 1
  move "REAPER arrange view (X)" $((VX+300)) $((VY+260))
  DISPLAY="$OUTER_DISPLAY" XAUTHORITY="$OUTER_XAUTHORITY" import -window root "$OUT/tooltip-after-leave-to-reaper.png"
} > "$OUT/view-path.txt"
wait "$CW"

# 2. Text entry: click the cutoff value, type, Enter.
count() { echo "$1: input $(grep -c 'iPlug2 input' /tmp/reaper.log) keycode $(grep -c keycode /tmp/reaper.log) bytes $(wc -c < /tmp/reaper.log)"; }
{
  count before
  outer xdotool mousemove $((VX+501)) $((VY+45)); sleep 0.3; outer xdotool mousedown 1; sleep 0.08; outer xdotool mouseup 1
  sleep 0.8; outer xdotool type --delay 90 2000; outer xdotool key Return; sleep 1
  count live
  gdb -p "$(cat /tmp/reaper.pid)" -batch -ex 'call (int)fflush(0)' >/dev/null 2>&1
  count "after fflush"
} > "$OUT/text-entry-counts.txt"
DISPLAY="$OUTER_DISPLAY" XAUTHORITY="$OUTER_XAUTHORITY" import -window root "$OUT/text-entry-result.png"
cp /tmp/reaper.log "$OUT/reaper-stderr.txt"
cat "$OUT/xev-path.txt" "$OUT/xev-crossings.txt" "$OUT/view-path.txt" "$OUT/view-crossings.txt" "$OUT/text-entry-counts.txt"
