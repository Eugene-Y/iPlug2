#!/usr/bin/env bash
# Does a plain X11 client get LeaveNotify when the pointer glides (small steps, not one jump) from its window onto the
# nested shell's desktop? Usage: nested-xev-glide.sh <outdir>   (through vm-x.sh, after nested-start.sh)
OUT="$1"; D="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
. "$D/nested-env.sh" || exit 1
nested_overview_off >/dev/null
outer() { DISPLAY="$OUTER_DISPLAY" XAUTHORITY="$OUTER_XAUTHORITY" "$@"; }
xev -geometry 200x150+600+300 -event mouse > "$OUT/xev-glide-raw.txt" 2>&1 &
XEV=$!
# xev sets no _NET_WM_PID; wmctrl lists the client, not mutter's frame of the same title.
for i in $(seq 20); do XW=$(wmctrl -l | awk '/ Event Tester$/{print $1}' | head -1); [ -n "$XW" ] && break; sleep 0.5; done
[ -n "$XW" ] || { echo "no xev window"; kill "$XEV"; exit 1; }
wmctrl -i -r "$XW" -e 0,600,300,-1,-1; sleep 1
read -r OX OY < <(outer xwininfo -id "$(outer xdotool search --pid "$NP" | while read -r w; do outer xwininfo -id "$w" | grep -q IsViewable && echo "$w"; done | head -1)" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}')
glide() { # from x1,y1 to x2,y2 in 40 steps of 25 ms
  for i in $(seq 0 40); do outer xdotool mousemove $(( $1 + ($3-$1)*i/40 )) $(( $2 + ($4-$2)*i/40 )); sleep 0.025; done
}
{
  echo "$(date +%T.%N | cut -c1-12) jump into xev"; outer xdotool mousemove $((OX+700)) $((OY+400)); sleep 1
  echo "$(date +%T.%N | cut -c1-12) glide to the nested desktop"; glide $((OX+700)) $((OY+400)) $((OX+700)) $((OY+700)); sleep 1
  echo "$(date +%T.%N | cut -c1-12) glide back into xev"; glide $((OX+700)) $((OY+700)) $((OX+700)) $((OY+400)); sleep 1
  echo "$(date +%T.%N | cut -c1-12) glide out of the nested compositor"; glide $((OX+700)) $((OY+400)) $((OX+700)) 860; sleep 1
} > "$OUT/xev-glide-path.txt"
kill "$XEV"; wait "$XEV" 2>/dev/null
grep -E "^(EnterNotify|LeaveNotify)" "$OUT/xev-glide-raw.txt" > "$OUT/xev-glide-crossings.txt"
cat "$OUT/xev-glide-path.txt" "$OUT/xev-glide-crossings.txt"; grep -c MotionNotify "$OUT/xev-glide-raw.txt"
