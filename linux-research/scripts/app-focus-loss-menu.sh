#!/usr/bin/env bash
# Review S3 (P3b): the focus leaves for another top-level window while a stand-in edit's right-click menu is
# expanded. Run INSIDE the GUI container against a Debug+ASan APP; prints any ASan report.
# Usage: app-focus-loss-menu.sh <app> <outdir>
set -u
APP="$1"; OUT="$2"; mkdir -p "$OUT"
pkill -x IPlugControls; pkill -x xmessage; sleep 0.5
ASAN_OPTIONS=detect_leaks=0 "$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
sleep 6
xmessage -geometry +1200+50 other-window >/dev/null 2>&1 &
sleep 1
W=$(xdotool search --onlyvisible --name '^IPlugControls$' | head -1)
read -r X Y < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
M=$(xdotool search --onlyvisible --class xmessage | head -1)
xdotool mousemove $((X+224)) $((Y+692)) click 1; sleep 0.3
xdotool mousemove $((X+224)) $((Y+720)) click 1; sleep 0.5
for i in 1 2 3; do
  xdotool mousemove $((X+941)) $((Y+437)) click --repeat 2 --delay 80 1; sleep 1
  xdotool mousemove $((X+941)) $((Y+437)) click 3; sleep 1
  import -window root "$OUT/s3-menu-$i.png"
  xdotool windowfocus "$M"; sleep 0.5
  xdotool mousemove $((X+600)) $((Y+500)); sleep 0.3
  xdotool mousemove $((X+945)) $((Y+478)) click 1; sleep 1
  xdotool mousemove $((X+500)) $((Y+400)) click 1; sleep 0.5
  pgrep -x IPlugControls >/dev/null || { echo "APP DEAD after round $i"; break; }
done
import -window root "$OUT/s3-end.png"
pkill -x IPlugControls; pkill -x xmessage; sleep 1
grep -m1 -A25 "ERROR: AddressSanitizer" "$OUT/stderr.txt" || echo "S3: no ASan report"
