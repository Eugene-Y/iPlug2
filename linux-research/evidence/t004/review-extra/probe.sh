#!/usr/bin/env bash
# Reviewer probe (T-004 round 1): signal quit paths the worker's harness does not cover.
# Runs INSIDE the GUI container from linux-research/. Usage: probe.sh <outdir> <app binary>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; : > "$OUT/checks.txt"
export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME"
trap 'rm -rf "$HOME/.cache" "$HOME/.config/pulse"' EXIT
win() { local w; for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }; done; }
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
finish() { # <label> <pid>
  for _ in $(seq 50); do kill -0 "$2" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$2" 2>/dev/null; then echo "$1: STILL RUNNING after 5 s" | tee -a "$OUT/checks.txt"; kill -9 "$2"; wait "$2"
  else wait "$2"; echo "$1: exit $?" | tee -a "$OUT/checks.txt"; fi
}
# 1. SIGTERM while the startup "Audio is off" box is up
"$APP" > "$OUT/stderr-1.txt" 2>&1 & P=$!; sleep 4
[ -n "$(win Audio)" ] && echo "1: Audio box up" >> "$OUT/checks.txt"
import -window root "$OUT/1-startup-box.png"
kill -TERM $P; finish "1 SIGTERM over startup box" $P
# 2. SIGTERM with the File menu open (no dialog)
"$APP" > "$OUT/stderr-2.txt" 2>&1 & P=$!; sleep 4
b=$(win Audio); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Return; sleep 0.8; }
read -r X Y < <(origin "$(win "$NAME")"); slow_click $((X + 16)) $((Y + 6))
import -window root "$OUT/2-file-menu.png"
kill -TERM $P; finish "2 SIGTERM with File menu open" $P
# 3. SIGINT over Help > About
"$APP" > "$OUT/stderr-3.txt" 2>&1 & P=$!; sleep 4
b=$(win Audio); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Return; sleep 0.8; }
read -r X Y < <(origin "$(win "$NAME")"); slow_click $((X + 90)) $((Y + 6)); slow_click $((X + 110)) $((Y + 29))
import -window root "$OUT/3-about.png"
kill -INT $P; finish "3 SIGINT over About" $P
# 4. SIGINT then SIGTERM in quick succession over Preferences
"$APP" > "$OUT/stderr-4.txt" 2>&1 & P=$!; sleep 4
b=$(win Audio); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Return; sleep 0.8; }
read -r X Y < <(origin "$(win "$NAME")"); slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29))
kill -INT $P; kill -TERM $P; finish "4 SIGINT+SIGTERM over Preferences" $P
