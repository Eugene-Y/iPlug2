#!/usr/bin/env bash
# T-004: the standalone APP without a usable audio device, its Preferences dialog and its quit paths.
# Runs INSIDE the GUI container. Usage: app-audio-test.sh <outdir> <app binary>
# Env: APP_ARGS (extra APP arguments), EXPECT_BOX=0 when audio should start (no "Audio" message box).
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
export LC_ALL=C.UTF-8
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; : > "$OUT/checks.txt"
FAILS=0
fail() { echo "FAIL: $*" | tee -a "$OUT/checks.txt"; FAILS=$((FAILS + 1)); }
pass() { echo "ok: $*" | tee -a "$OUT/checks.txt"; }
pkill -x "$NAME"; sleep 0.5

# A fresh HOME without ~/.config: the APP must create its settings folder itself.
export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME"
export TMPDIR="$OUT" # where Debug > Save Screenshot writes
# Only the settings files are evidence; Mesa's shader cache and PulseAudio's auth cookie are not
tidy_home() { rm -rf "$HOME/.cache" "$HOME/.config/pulse"; }
trap tidy_home EXIT
INI="$HOME/.config/$NAME/settings.ini"

# By WM_CLASS too: Mutter's frame windows carry their client's title
win() {
  local w
  for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do
    xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }
  done
}
wait_win() { for _ in $(seq 50); do w=$(win "$1"); [ -n "$w" ] && { sleep 0.5; echo "$w"; return 0; }; sleep 0.2; done; return 1; }
# SWELL's menu bar ignores a click whose press and release share a timestamp
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
client_origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
menu_item() { # <row>: File menu item, 0 = Preferences, 1 = Quit
  read -r X Y < <(client_origin "$(win "$NAME")")
  slow_click $((X + 16)) $((Y + 6))
  slow_click $((X + 41)) $((Y + 29 + 20 * $1))
}

# shellcheck disable=SC2086
"$APP" ${APP_ARGS:-} > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
kill -0 "$APID" 2>/dev/null && pass "APP running after 5 s" || fail "APP died at start: $(tail -3 "$OUT/stderr.txt")"
[ -f "$INI" ] && pass "settings created at ${INI#"$OUT"/}" || fail "no $INI"
import -window root "$OUT/01-start.png"

if [ "${EXPECT_BOX:-1}" = 1 ]; then
  if B=$(wait_win Audio); then
    pass "audio message: $(grep -a -m1 'Audio is off' "$OUT/stderr.txt")"
    xdotool windowactivate --sync "$B"; xdotool key Return; sleep 1
  else
    fail "no Audio message box"
  fi
else
  [ -z "$(win Audio)" ] && pass "no audio message box" || fail "unexpected Audio box: $(grep -a 'Audio is off' "$OUT/stderr.txt")"
fi

menu_item 0
if P=$(win 'Preferences.*') && [ -n "$P" ]; then
  pass "Preferences open, title '$(xdotool getwindowname "$P")'"
  import -window root "$OUT/02-prefs.png"
  xdotool windowactivate --sync "$P"; xdotool key Escape; sleep 1
  [ -z "$(win 'Preferences.*')" ] && pass "Preferences closed by Escape" || fail "Preferences still open"
else
  fail "Preferences did not open"
fi

# Menu shortcuts reach the APP also when the plug-in UI has the keyboard focus (it passes on unused keys)
read -r X Y < <(client_origin "$(win "$NAME")")
slow_click $((X + 500)) $((Y + 300))
xdotool key ctrl+comma; sleep 1.5
if P=$(win 'Preferences.*') && [ -n "$P" ]; then
  pass "Ctrl+, from the plug-in UI opens Preferences"
  xdotool windowactivate --sync "$P"; xdotool key Escape; sleep 1
else
  fail "Ctrl+, did not open Preferences"
fi
rm -f "$OUT"/*_screenshot_*.png
slow_click $((X + 500)) $((Y + 300))
xdotool key ctrl+shift+s; sleep 2
S=$(ls "$OUT"/*_screenshot_*.png 2>/dev/null | head -1)
[ -n "$S" ] && pass "Ctrl+Shift+S saved $(basename "$S") ($(identify -format '%wx%h' "$S"))" || fail "Ctrl+Shift+S saved no screenshot"
B=$(win 'Screenshot Saved'); [ -n "$B" ] && { xdotool windowactivate --sync "$B"; xdotool key Escape; sleep 0.8; }

menu_item 1
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after File > Quit"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "File > Quit, exit 0" || fail "File > Quit, exit $RC"; fi

# SIGINT (Ctrl+C in a terminal) and SIGTERM close the window like a quit, also over open dialogs
signal_quit() { # <signal> <what is open: none|prefs|box (the startup message)>
  "$APP" ${APP_ARGS:-} > "$OUT/stderr-sig$1-$2.txt" 2>&1 &
  APID=$!
  sleep 4
  [ "$2" != box ] && [ -n "$(win Audio)" ] && { xdotool windowactivate --sync "$(win Audio)"; xdotool key Return; sleep 0.5; }
  if [ "$2" = prefs ]; then
    menu_item 0
    P=$(win 'Preferences.*')
    read -r X Y < <(client_origin "$P"); slow_click $((X + 150)) $((Y + 553)) # Apply: a message box over Preferences, if audio fails
    import -window root "$OUT/03-before-sig$1.png"
  fi
  kill -"$1" "$APID"
  for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after SIG$1 ($2 open)"; kill -9 "$APID"
  else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "SIG$1 with $2 open, exit 0" || fail "SIG$1 with $2 open, exit $RC"; fi
}
signal_quit INT none
signal_quit TERM prefs
[ "${EXPECT_BOX:-1}" = 1 ] && signal_quit TERM box

if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; fi
