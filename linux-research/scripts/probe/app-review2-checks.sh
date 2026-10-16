#!/usr/bin/env bash
# T-004 review round 2 checks (Docker GUI container, jackd dummy with outputs only):
#  m6 Debug > Save Screenshot from the menu on X without a compositor; m5 JACK + Apply + Cancel keeps the INI;
#  m7 the sample-rate list of an output-only device. Usage: app-review2-checks.sh <outdir> <app>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
export HOME="$OUT/home" TMPDIR="$OUT"; rm -rf "$HOME" "$OUT"/*_screenshot_*.png; mkdir -p "$HOME"
INI="$HOME/.config/$NAME/settings.ini"
win() { local w; for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do
  xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }; done; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
dismiss() { local b; b=$(win "$1"); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Escape; sleep 0.8; }; }

pkill -x "$NAME"; pkill jackd; sleep 0.5
jackd --no-realtime -d dummy -r 48000 -p 256 -C 0 -P 2 > "$OUT/jackd.log" 2>&1 &
sleep 2
"$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
dismiss Audio # PulseAudio, the default, has no server here
read -r X Y < <(origin "$(win "$NAME")")

# m6: the Debug menu's last item, chosen with the keyboard so the menu covers the UI until the command runs
slow_click $((X + 41)) $((Y + 6)); xdotool key Up Return; sleep 3
S=$(ls "$OUT"/*_screenshot_*.png 2>/dev/null | head -1)
[ -n "$S" ] && { cp "$S" "$OUT/m6-menu-screenshot.png"; echo "m6: saved, black pixels in the menu's area: $(convert "$S" -crop 200x120+0+0 -fill white +opaque black -format '%[fx:int(w*h*(1-mean))]' info:)"; } || echo "m6: no screenshot"
dismiss 'Screenshot Saved'

# m5 + m7: JACK, Apply, look at the sample rate, Cancel
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5
P=$(win 'Preferences.*'); read -r PX PY < <(origin "$P")
slow_click $((PX + 131)) $((PY + 75)); slow_click $((PX + 83)) $((PY + 112)) # JACK
slow_click $((PX + 150)) $((PY + 553)); sleep 1 # Apply
dismiss Audio
import -window root "$OUT/m7-prefs-after-apply.png"
echo "after Apply: $(jack_lsp 2>/dev/null | grep -c "^$NAME:outport") JACK ports"
P=$(win 'Preferences.*'); xdotool windowactivate --sync "$P"; slow_click $((PX + 362)) $((PY + 553)); sleep 1 # Cancel
echo "m5: settings.ini after Cancel: $(grep -E '^(driver|sr)=' "$INI" | tr '\n' ' ')"
kill -TERM "$APID"; wait "$APID"; pkill jackd
rm -rf "$HOME/.cache"
