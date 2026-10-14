#!/usr/bin/env bash
# APP under the session's window manager: after a click into the editor, who has the X focus, and does Escape reach
# the editor (the about box) or SWELL (whose Escape closes the APP's dialog = the app)?
# Usage: app-focus.sh <out> <app-binary> [trials=3] [hold=0.08|click] [elsewhere=1]
#   hold=click: xdotool's click (press and release in the same millisecond, as xact.sh clicks)
#   elsewhere=0: do not click the desktop first (the APP keeps the WM focus from its previous click)
#   elsewhere=edit: like 0, after an edit committed with Return (the sequence that ends the APP P3b script)
set -uo pipefail
OUT="$1"; APP="$2"; N="${3:-3}"; HOLD="${4:-0.08}"; ELSEWHERE="${5:-1}"; NAME=$(basename "$APP")
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
gcc -O1 -include stdlib.h -o /tmp/focuswatch "$HERE/probe/focuswatch.c" -lX11 2>/dev/null
for t in $(seq "$N"); do
  pkill -x "$NAME" 2>/dev/null; sleep 0.5
  ( cd "$(dirname "$APP")" && setsid "$APP" --no-io > "$HERE/../$OUT/stderr-$t.txt" 2>&1 < /dev/null & )
  sleep 4
  PID=$(pgrep -x "$NAME" | head -1)
  TOP=$(for w in $(xdotool search --onlyvisible --name "^$NAME\$"); do xprop -id "$w" WM_CLASS | grep -q "\"$NAME\"" && echo "$w"; done | head -1)
  read -r OX OY < <(xwininfo -id "$TOP" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
  # focus elsewhere first (the desktop), then click the editor's about button as a person would
  if [ "$ELSEWHERE" = 1 ]; then xdotool mousemove 1300 700 click 1; else xdotool mousemove $((OX + 1010)) $((OY + 600)) click 1; fi; sleep 0.5
  if [ "$ELSEWHERE" = edit ]; then
    xdotool mousemove $((OX + 941)) $((OY + 437)) click --repeat 2 --delay 80 1; sleep 1; xdotool type 5; xdotool key Return; sleep 1
  fi
  timeout 3 /tmp/focuswatch 2 > "$OUT/focus-$t.txt" 2>&1 &
  if [ "$HOLD" = click ]; then xdotool mousemove $((OX + 993)) $((OY + 737)) click 1
  else xdotool mousemove $((OX + 993)) $((OY + 737)) mousedown 1; sleep "$HOLD"; xdotool mouseup 1; fi
  sleep 1.5
  xdotool key Escape; sleep 1
  if ps -o stat= -p "$PID" >/dev/null; then st="APP alive after Escape"; else st="APP CLOSED by Escape (it went to SWELL)"; fi
  wait
  log "trial $t (hold $HOLD, elsewhere $ELSEWHERE): $st; focus trail: $(awk '{print $3}' "$OUT/focus-$t.txt" | tr '\n' ' ')"
  log "  editor trace: $(grep -E 'focus|Focus|keycode 9' "$OUT/stderr-$t.txt" | tr '\n' ';' | cut -c1-600)"
  ps -o stat= -p "$PID" >/dev/null && { wmctrl -i -c "$TOP"; sleep 1.5; pkill -x "$NAME" 2>/dev/null; }
done
