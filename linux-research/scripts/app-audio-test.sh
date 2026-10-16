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
INI="$HOME/.config/$NAME/settings.ini"

win() { xdotool search --onlyvisible --name "^$1\$" 2>/dev/null | head -1; }
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
if P=$(xdotool search --onlyvisible --name '^Preferences' | head -1) && [ -n "$P" ]; then
  pass "Preferences open, title '$(xdotool getwindowname "$P")'"
  import -window root "$OUT/02-prefs.png"
  xdotool windowactivate --sync "$P"; xdotool key Escape; sleep 1
  [ -z "$(xdotool search --onlyvisible --name '^Preferences' | head -1)" ] && pass "Preferences closed by Escape" || fail "Preferences still open"
else
  fail "Preferences did not open"
fi

menu_item 1
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after File > Quit"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "File > Quit, exit 0" || fail "File > Quit, exit $RC"; fi

# SIGINT (Ctrl+C in a terminal) closes the window like a quit
"$APP" ${APP_ARGS:-} > "$OUT/stderr-sigint.txt" 2>&1 &
APID=$!
sleep 4
[ -n "$(win Audio)" ] && { xdotool windowactivate --sync "$(win Audio)"; xdotool key Return; sleep 0.5; }
kill -INT "$APID"
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after SIGINT"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "SIGINT, exit 0" || fail "SIGINT, exit $RC"; fi

if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; fi
