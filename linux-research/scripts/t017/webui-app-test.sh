#!/usr/bin/env bash
# T-017: the IPlugWebUI standalone app (WebView editor in the SWELL window). Runs INSIDE the GUI container.
# Usage: webui-app-test.sh <outdir> <app-binary>   (APP_SCALE=2: the page's offsets in pixels at Xft.dpi 192)
# Checks: render, typing into a text field (added to a copy of the page), JS -> C++ resize of the app window,
# helper killed (the app survives and says so), the app quitting leaves no helper or WebKit process.
set -uo pipefail
OUT="$1"; APP="$2"; S="${APP_SCALE:-1}"
mkdir -p "$OUT"
FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
helpers() { pgrep -fc '[/]iplug2-webview-helper' || true; }
webkits() { pgrep -fc '[W]ebKit(Web|Network)Process' || true; }
shot() { import -window root "$OUT/$1.png"; }
D=/tmp/t017-app; rm -rf "$D"; mkdir -p "$D"
cp -a "$APP" "$APP.resources" "$D/"
NAME=$(basename "$APP")
sed -i 's|<input type="file" id="textfile" name="textfile" accept="text/\*" />|&<input type="text" id="t017text" size="14" style="margin-left:8px">|' "$D/$NAME.resources/web/index.html"
HOME=/tmp/t017-apphome; rm -rf "$HOME"; mkdir -p "$HOME"; export HOME
stdbuf -oL -eL "$D/$NAME" --no-io > "$OUT/app.log" 2>&1 &
APID=$!
for i in $(seq 100); do W=$(xdotool search --onlyvisible --name "^$NAME" 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.1; done
sleep 6
[ -n "$W" ] || { fail "no app window"; }
eval "$(xdotool getwindowgeometry --shell "$W")"; echo "app window $W at $X,$Y ${WIDTH}x$HEIGHT"
shot a01-open
# the editor's container: the largest child of the app window
read -r OX OY OW OH < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }')
echo "editor at $OX,$OY ${OW}x$OH"
convert "$OUT/a01-open.png" -crop "${OW}x${OH}+${OX}+${OY}" +repage "$OUT/a01-view.png"
SD=$(convert "$OUT/a01-view.png" -colorspace Gray -format '%[fx:standard_deviation]' info:)
awk -v s="$SD" 'BEGIN{exit !(s < 0.02)}' && fail "editor blank (stddev $SD)" || echo "render ok (stddev $SD)"
[ "$(helpers)" = 1 ] || fail "helpers after open: $(helpers)"
xdotool mousemove $((OX + 437 * S)) $((OY + 259 * S)) click 1; sleep 0.5
xdotool type --delay 60 "typed in the app"; sleep 0.5
shot a02-typed; convert "$OUT/a02-typed.png" -crop "$((560 * S))x$((60 * S))+$((OX + 20 * S))+$((OY + 230 * S))" +repage "$OUT/a02-typed-field.png"
xdotool mousemove $((OX + 321 * S)) $((OY + 147 * S)) click 1; sleep 2
eval "$(xdotool getwindowgeometry --shell "$W")"; echo "after Large GUI: ${WIDTH}x$HEIGHT"
[ "$WIDTH" -ge $((1024 * S)) ] || fail "Large GUI did not grow the app window (${WIDTH}x$HEIGHT)"
shot a03-large
pkill -9 -f '[/]iplug2-webview-helper'; sleep 2
kill -0 "$APID" 2>/dev/null && echo "app alive after the helper was killed" || fail "app died with its helper"
shot a04-killed
# never xdotool windowclose (XDestroyWindow on another client's window breaks GNOME's frames): a polite close
if command -v wmctrl >/dev/null; then wmctrl -i -c "$W"; else kill "$APID"; fi
for i in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
kill -0 "$APID" 2>/dev/null && { fail "app did not quit"; kill -9 "$APID"; }
sleep 2
echo "after quit: helpers $(helpers), WebKit $(webkits)"
[ "$(helpers)" = 0 ] && [ "$(webkits)" = 0 ] || fail "processes left after the app quit: helpers $(helpers), WebKit $(webkits)"
if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
