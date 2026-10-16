#!/usr/bin/env bash
# Screenshots of the APP's first frames with the pointer away, then after hovering the header logo.
# Usage: vm-run.sh vm/probe/app-first-frame.sh <out> <app>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
xdotool mousemove 5 850
"$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 2; import -window root "$OUT/01-2s.png"
sleep 3; import -window root "$OUT/02-5s.png"
W=""; for w in $(xdotool search --onlyvisible --name "^$NAME\$"); do xprop -id "$w" WM_CLASS | grep -q "\"$NAME\"" && W=$w; done
read -r X Y < <(xwininfo -id "$W" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}')
xdotool mousemove $((X + 50)) $((Y + 70)); sleep 1; import -window root "$OUT/03-hover.png"
xdotool mousemove 5 850; sleep 1; import -window root "$OUT/04-away.png"
kill -TERM "$APID"; wait "$APID"
