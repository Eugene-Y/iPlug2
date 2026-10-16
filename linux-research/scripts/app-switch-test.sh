#!/usr/bin/env bash
# T-004: switch the APP's audio driver in Preferences without a restart, play a note, quit; then the
# threads left at exit(). Runs INSIDE the GUI container with a JACK server running (jackd -d dummy).
# Usage: app-switch-test.sh <outdir> <app binary> [driver to switch to and back from: ALSA|PulseAudio]
# Env: EXPECT_OTHER=off (default: the other driver has no device) or on (it plays too).
set -uo pipefail
OUT="$1"; APP="$2"; OTHER="${3:-ALSA}"; NAME=$(basename "$APP")
export LC_ALL=C.UTF-8
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; : > "$OUT/checks.txt"
FAILS=0
fail() { echo "FAIL: $*" | tee -a "$OUT/checks.txt"; FAILS=$((FAILS + 1)); }
pass() { echo "ok: $*" | tee -a "$OUT/checks.txt"; }
pkill -x "$NAME"; sleep 0.5

export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME/.config/$NAME"
INI="$HOME/.config/$NAME/settings.ini"
printf '[audio]\ndriver=1\nindev=system\noutdev=system\nsr=48000\nbuffer=256\n' > "$INI"

win() { xdotool search --onlyvisible --name "^$1\$" 2>/dev/null | head -1; }
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
jack_ports() { jack_lsp 2>/dev/null | grep -c "^$NAME:"; }
dismiss_audio_box() { local b; b=$(win Audio); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Return; sleep 0.8; }; [ -n "$b" ]; }
# The meter's lowest segment, green while the instrument sounds
meter_pixel() { read -r X Y < <(origin "$(win "$NAME")"); import -window root -crop "1x1+$((X + 943))+$((Y + 294))" txt:- | tail -1 | awk '{print $3}'; }
prefs_open() {
  read -r X Y < <(origin "$(win "$NAME")")
  slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5
  P=$(xdotool search --onlyvisible --name '^Preferences' | head -1)
}
prefs_driver() { # <row in the driver list: 0 ALSA, 1 JACK, 2 PulseAudio>
  read -r X Y < <(origin "$P")
  slow_click $((X + 131)) $((Y + 75)); slow_click $((X + 83)) $((Y + 96 + 16 * $1))
}
prefs_button() { read -r X Y < <(origin "$P"); slow_click $((X + $1)) $((Y + 553)); sleep 1; }
APPLY=150; OK=256
case "$OTHER" in ALSA) ROW=0;; PulseAudio) ROW=2;; *) echo "unknown driver $OTHER"; exit 2;; esac

"$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
[ "$(jack_ports)" -ge 2 ] && pass "JACK: $(jack_lsp | grep "^$NAME:" | tr '\n' ' ')" || fail "no $NAME ports in JACK: $(jack_lsp | tr '\n' ' ')"
dismiss_audio_box && fail "Audio box at start on JACK"

read -r X Y < <(origin "$(win "$NAME")")
SILENT=$(meter_pixel)
xdotool mousemove $((X + 211)) $((Y + 614)) mousedown 1; sleep 0.5
LOUD=$(meter_pixel); import -window root "$OUT/01-jack-note.png"; xdotool mouseup 1; sleep 0.5
[ "$SILENT" != "$LOUD" ] && pass "note on JACK moves the meter ($SILENT -> $LOUD)" || fail "meter unchanged ($SILENT) on JACK"

prefs_open
[ -n "$P" ] && pass "Preferences '$(xdotool getwindowname "$P")'" || fail "Preferences did not open"
prefs_driver "$ROW"; prefs_button $APPLY
import -window root "$OUT/02-$OTHER-applied.png"
if [ "${EXPECT_OTHER:-off}" = off ]; then
  dismiss_audio_box && pass "$OTHER: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)" || fail "no Audio box after switching to $OTHER"
  T=$(xdotool getwindowname "$P"); [[ "$T" == *"no device found"* ]] && pass "title '$T'" || fail "title '$T'"
else
  dismiss_audio_box && fail "Audio box after switching to $OTHER: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
  T=$(xdotool getwindowname "$P"); [ "$T" = Preferences ] && pass "$OTHER plays, title '$T'" || fail "title '$T'"
fi
[ "$(jack_ports)" = 0 ] && pass "JACK client gone after switching to $OTHER" || fail "JACK ports left: $(jack_ports)"

prefs_driver 1; prefs_button $APPLY
dismiss_audio_box && fail "Audio box after switching back to JACK: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
T=$(xdotool getwindowname "$P"); [ "$T" = Preferences ] && pass "back on JACK, title '$T'" || fail "title '$T'"
[ "$(jack_ports)" -ge 2 ] && pass "JACK ports back" || fail "no JACK ports after switching back"
import -window root "$OUT/03-jack-again.png"
prefs_button $OK
[ -z "$(xdotool search --onlyvisible --name '^Preferences' | head -1)" ] && pass "OK closes Preferences" || fail "Preferences still open"
grep -q '^driver=1$' "$INI" && pass "settings.ini: driver=1 (JACK)" || fail "settings.ini: $(grep driver "$INI")"

xdotool mousemove $((X + 211)) $((Y + 614)) mousedown 1; sleep 0.5
LOUD=$(meter_pixel); xdotool mouseup 1
[ "$SILENT" != "$LOUD" ] && pass "note after the switch moves the meter" || fail "meter unchanged after the switch"

read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after File > Quit"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "File > Quit with JACK running, exit 0" || fail "File > Quit, exit $RC"; fi

# Threads still alive when main() returns: only the main thread is ours to account for
cat > "$OUT/exit-threads.gdb" <<'EOF'
set pagination off
break exit
run
info threads
thread apply all bt 3
continue
EOF
gdb -q -batch -x "$OUT/exit-threads.gdb" "$APP" > "$OUT/exit-threads.txt" 2>&1 &
GPID=$!
sleep 8
read -r X Y < <(origin "$(win "$NAME")")
xdotool mousemove $((X + 211)) $((Y + 614)) mousedown 1; sleep 0.3; xdotool mouseup 1
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 100); do kill -0 "$GPID" 2>/dev/null || break; sleep 0.1; done
kill -0 "$GPID" 2>/dev/null && { fail "gdb run did not finish"; kill -9 "$GPID"; pkill -x "$NAME"; }
N=$(awk '/^  Id +Target Id/{f=1;next} f&&/^[* ] +[0-9]+ +Thread/{n++} END{print n+0}' "$OUT/exit-threads.txt")
awk '/^  Id +Target Id/{f=1} f&&/^[* ] +[0-9]+ +Thread/' "$OUT/exit-threads.txt" | sed 's/^/  /' | tee -a "$OUT/checks.txt"
[ "$N" -ge 1 ] && pass "$N thread(s) at exit()" || fail "no thread list at exit() (see exit-threads.txt)"

if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; fi
