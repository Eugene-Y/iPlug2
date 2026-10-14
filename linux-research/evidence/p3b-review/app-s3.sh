#!/usr/bin/env bash
# S3 of the P3b review, inside the GUI container: the focus leaves for another top-level window while a stand-in
# edit's right-click menu is open (the edit commits on the focus loss; the menu refers to the text entry control).
# Then a choice in that menu, and a second edit. Usage: app-s3.sh <app> <outdir>
set -u
APP="$1"; OUT="$2"; mkdir -p "$OUT"
pkill -x IPlugControls; pkill -x xmessage; sleep 0.5
ASAN_OPTIONS=detect_leaks=0 "$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
sleep 6
W=$(xdotool search --onlyvisible --name '^IPlugControls$' | head -1)
read -r X Y < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
echo "view $X $Y"
c() { xdotool mousemove $((X+$1)) $((Y+$2)) click ${3:-1}; }
c 224 692; sleep 0.3; c 224 720; sleep 0.5
xmessage -geometry +1300+50 other-window & sleep 1
M=$(xdotool search --onlyvisible --name '^xmessage$' | head -1)
printf 's3 clipboard' | xclip -selection clipboard
xdotool mousemove $((X+941)) $((Y+437)) click --repeat 2 --delay 80 1; sleep 1
c 941 437 3; sleep 1
import -window root "$OUT/s3-1-menu-open.png"
xdotool windowactivate --sync "$M" 2>/dev/null || xdotool windowfocus "$M"; sleep 1
echo "focus now: $(xdotool getwindowfocus)  xmessage: $M"
import -window root "$OUT/s3-2-focus-left.png"
c 945 478; sleep 1
import -window root "$OUT/s3-3-menu-choice.png"
xdotool mousemove $((X+941)) $((Y+437)) click --repeat 2 --delay 80 1; sleep 1
xdotool type 5; xdotool key Return; sleep 1
import -window root "$OUT/s3-4-second-edit.png"
pgrep -x IPlugControls >/dev/null && echo "APP alive" || echo "APP DEAD"
pkill -x IPlugControls; pkill -x xmessage; sleep 1
grep -m1 -A20 "ERROR: AddressSanitizer" "$OUT/stderr.txt" || echo "no ASan report"
