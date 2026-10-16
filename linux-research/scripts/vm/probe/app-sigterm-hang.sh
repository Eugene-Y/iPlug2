#!/usr/bin/env bash
# T-004 probe: the APP with Preferences open (after Apply) gets SIGTERM; if it is still alive after 5 s, dump all
# thread stacks. Usage: vm-run.sh vm/probe/app-sigterm-hang.sh <out> <app>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME"
# Only the settings files are evidence; Mesa's shader cache and PulseAudio's auth cookie are not
tidy_home() { rm -rf "$HOME/.cache" "$HOME/.config/pulse"; }
trap tidy_home EXIT
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
win() { local w; for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do
  xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }; done; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }

"$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5
P=$(win 'Preferences.*')
[ -n "${NO_APPLY:-}" ] || { read -r PX PY < <(origin "$P"); slow_click $((PX + 150)) $((PY + 553)); sleep 1; }
kill -TERM "$APID"
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then
  echo "HUNG after SIGTERM"
  import -window root "$OUT/hung.png"
  sudo -n /usr/bin/gdb -q -batch -p "$APID" -ex "info threads" -ex "thread apply all bt 12" > "$OUT/hung-bt.txt" 2>&1
  kill -9 "$APID"
else
  wait "$APID"; echo "exited rc=$?"
fi
