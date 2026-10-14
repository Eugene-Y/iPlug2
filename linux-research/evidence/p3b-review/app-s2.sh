#!/usr/bin/env bash
# S2 of the P3b review, inside the GUI container: the plug-in removes the popup control while the stand-in menu
# is still collapsing. One xdotool call, so the three clicks land within the collapse animation.
# Usage: app-s2.sh <app> <outdir>
set -u
APP="$1"; OUT="$2"; mkdir -p "$OUT"
pkill -x IPlugControls; sleep 0.5
ASAN_OPTIONS=detect_leaks=0 "$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
sleep 6
W=$(xdotool search --onlyvisible --name '^IPlugControls$' | head -1)
read -r X Y < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
echo "view $X $Y"
xdotool mousemove $((X+224)) $((Y+692)) click 1; sleep 0.3
xdotool mousemove $((X+224)) $((Y+720)) click 1; sleep 0.5
for i in 1 2 3; do
  xdotool mousemove $((X+828)) $((Y+284)) click 1; sleep 1
  xdotool mousemove $((X+833)) $((Y+358)) click 1 mousemove $((X+220)) $((Y+720)) click 1 mousemove $((X+240)) $((Y+722)) click 1
  sleep 1.5
  pgrep -x IPlugControls >/dev/null || { echo "APP DEAD after round $i"; break; }
done
import -window root "$OUT/s2-end.png"
pkill -x IPlugControls; sleep 1
grep -m1 -A25 "ERROR: AddressSanitizer" "$OUT/stderr.txt" || echo "no ASan report"
