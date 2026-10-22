#!/usr/bin/env bash
# T-022: the APP's MIDI follows its audio driver. Runs INSIDE the GUI container with a JACK server running
# (jackd -d dummy) and no ALSA sequencer. jack_midiseq loops a note into the APP's MIDI input; jack_midi_dump receives
# what IPlugInstrument echoes to its MIDI output. In Preferences: JACK -> ALSA -> Cancel, then PulseAudio -> JACK (the
# saved ports reconnect) and "off" and the port picked from the MIDI input list; File > Quit; the threads left at
# exit(); last, a start with the JACK server stopped keeps the saved ports, Apply after the server is up reconnects
# them, and OK reconnects them after the source client and then the JACK server restart.
# Usage: app-jack-midi-test.sh <outdir> <IPlugInstrument APP>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
export LC_ALL=C.UTF-8
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; : > "$OUT/checks.txt"
FAILS=0
fail() { echo "FAIL: $*" | tee -a "$OUT/checks.txt"; FAILS=$((FAILS + 1)); }
pass() { echo "ok: $*" | tee -a "$OUT/checks.txt"; }
info() { echo "info: $*" | tee -a "$OUT/checks.txt"; }
pkill -x "$NAME"; pkill -x jack_midiseq; pkill -x jack_midi_dump; sleep 0.5

SRC=seq:out
DUMP=midi-monitor:input
# A short note every second, so the meter rises and falls while MIDI reaches the instrument
jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 &
SEQPID=$!
jack_midi_dump > "$OUT/midi-out.txt" 2> /dev/null &
DUMPPID=$!

export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME/.config/$NAME"
# Only the settings files are evidence; Mesa's shader cache and PulseAudio's auth cookie are not
cleanup() { rm -rf "$HOME/.cache" "$HOME/.config/pulse"; kill "$SEQPID" "$DUMPPID" 2> /dev/null; }
trap cleanup EXIT
INI="$HOME/.config/$NAME/settings.ini"
printf '[audio]\ndriver=1\nindev=system\noutdev=system\nsr=48000\nbuffer=256\n[midi]\nindev=%s\noutdev=%s\ninchan=0\noutchan=0\n' \
  "$SRC" "$DUMP" > "$INI"

# By WM_CLASS too: Mutter's frame windows carry their client's title
win() {
  local w
  for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do
    xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }
  done
}
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
dismiss_audio_box() { local b; b=$(win Audio); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Return; sleep 0.8; }; [ -n "$b" ]; }
app_midi_ports() { jack_lsp | grep "^$NAME MIDI " | tr '\n' ' '; }
# jack_lsp -c lists each port, then its connections indented
connected() { jack_lsp -c | awk -v p="$1" -v q="$2" '$0==p{f=1;next} /^[^ \t]/{f=0} f&&index($0,q){found=1} END{exit !found}'; }
# The meter's lowest segment
meter_pixel() { local X Y; read -r X Y < <(origin "$(win "$NAME")"); import -window root -crop "1x1+$((X + 943))+$((Y + 294))" txt:- | tail -1 | awk '{print $3}'; }
meter_moves() {
  local first px i
  first=$(meter_pixel)
  for i in $(seq 25); do
    sleep 0.1; px=$(meter_pixel)
    [ "$px" != "$first" ] && { [ -n "${1:-}" ] && import -window root "$1"; echo "$first -> $px"; return 0; }
  done
  echo "$first"; return 1
}
prefs_open() {
  read -r X Y < <(origin "$(win "$NAME")")
  slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5
  P=$(win 'Preferences.*')
}
prefs_driver() { # <row in the driver list: 0 ALSA, 1 JACK, 2 PulseAudio>
  read -r X Y < <(origin "$P")
  slow_click $((X + 131)) $((Y + 75)); slow_click $((X + 83)) $((Y + 96 + 16 * $1))
}
prefs_button() { read -r X Y < <(origin "$P"); slow_click $((X + $1)) $((Y + 553)); sleep 1; }
APPLY=150; OK=256; CANCEL=362
MIDI_IN=427; MIDI_OUT=484
# Opens a MIDI device list, shoots it, and closes it with Escape or picks <row> (0 is "off")
midi_list() { # <combo y> <png> [row]
  read -r X Y < <(origin "$P")
  slow_click $((X + 121)) $((Y + $1)); import -window root "$2"
  if [ -n "${3:-}" ]; then slow_click $((X + 60)) $((Y + $1 + 21 + 16 * $3)); else xdotool key Escape; sleep 0.5; fi
}
# jackd takes a while to exit with clients attached, and a new one fails while the old one is still there
jack_down() { pkill -x jackd; for _ in $(seq 150); do pgrep -x jackd > /dev/null || return 0; sleep 0.1; done; pkill -9 -x jackd; sleep 0.5; }
ini() { awk -F= -v s="[$1]" -v k="$2" '$0==s{f=1;next} /^\[/{f=0} f&&$1==k{print; exit}' "$INI"; }

env "$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
dismiss_audio_box && fail "Audio box at start on JACK: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
[ "$(jack_lsp | grep -c "^$NAME:outport ")" -ge 2 ] && pass "the audio client keeps the name $NAME" || fail "audio ports: $(jack_lsp | tr '\n' ' ')"
info "the APP's JACK MIDI ports: $(app_midi_ports)"
connected "$SRC" "$NAME MIDI in:" && pass "$SRC -> $NAME MIDI in" || fail "$SRC not connected to the APP"
connected "$DUMP" "$NAME MIDI out:" && pass "$NAME MIDI out -> $DUMP" || fail "the APP's MIDI output not connected to $DUMP"
M=$(meter_moves "$OUT/01-jack-midi-note.png") && pass "JACK MIDI note moves the meter ($M)" || fail "meter unchanged ($M) with $SRC"
N=$(grep -c ' 90 3c ' "$OUT/midi-out.txt"); [ "$N" -ge 1 ] && pass "$N echoed note-ons at $DUMP" || fail "nothing echoed to $DUMP"

prefs_open
[ -n "$P" ] && pass "Preferences '$(xdotool getwindowname "$P")'" || fail "Preferences did not open"
midi_list $MIDI_IN "$OUT/02-jack-midi-inputs.png"
midi_list $MIDI_OUT "$OUT/03-jack-midi-outputs.png"

prefs_driver 0
import -window root "$OUT/04-alsa-selected.png"
[ -z "$(app_midi_ports)" ] && pass "ALSA selected: no JACK MIDI ports left" || fail "JACK MIDI ports left on ALSA: $(app_midi_ports)"
[ "$(ini audio driver)" = driver=1 ] && [ "$(ini midi indev)" = "indev=$SRC" ] && [ "$(ini midi outdev)" = "outdev=$DUMP" ] \
  && pass "ALSA only selected: settings.ini untouched, the saved ports kept" || fail "settings.ini while ALSA is selected: $(ini audio driver) $(ini midi indev) $(ini midi outdev)"
prefs_button $CANCEL
dismiss_audio_box && fail "Audio box after Cancel: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
[ -z "$(win 'Preferences.*')" ] && pass "Cancel closes Preferences" || fail "Preferences still open"
[ "$(ini audio driver)" = driver=1 ] && [ "$(ini midi indev)" = "indev=$SRC" ] && [ "$(ini midi outdev)" = "outdev=$DUMP" ] \
  && pass "settings.ini after Cancel: $(ini audio driver) $(ini midi indev) $(ini midi outdev)" || fail "settings.ini after Cancel: $(ini audio driver) $(ini midi indev) $(ini midi outdev)"
connected "$SRC" "$NAME MIDI in:" && pass "after Cancel: $SRC -> $NAME MIDI in" || fail "after Cancel: $SRC not connected"
M=$(meter_moves) && pass "after Cancel the note moves the meter ($M)" || fail "meter unchanged after Cancel ($M)"

prefs_open
prefs_driver 2
[ -z "$(app_midi_ports)" ] && pass "PulseAudio selected: no JACK MIDI ports left" || fail "JACK MIDI ports left on PulseAudio: $(app_midi_ports)"
prefs_driver 1
import -window root "$OUT/05-jack-again.png"
connected "$SRC" "$NAME MIDI in:" && connected "$DUMP" "$NAME MIDI out:" \
  && pass "back on JACK the saved ports reconnect: $SRC -> $NAME MIDI in, $NAME MIDI out -> $DUMP" || fail "saved ports not reconnected: $(jack_lsp -c | tr '\n' ' ')"
midi_list $MIDI_OUT "$OUT/06-jack-again-midi-outputs.png"
midi_list $MIDI_IN "$OUT/07-jack-again-midi-inputs.png" 0
connected "$SRC" "$NAME MIDI in:" && fail "\"off\" picked, $SRC still connected" || pass "\"off\" picked from the list: $SRC disconnected"
midi_list $MIDI_IN "$OUT/08-jack-again-midi-inputs-off.png" 1
import -window root "$OUT/09-input-picked.png"
connected "$SRC" "$NAME MIDI in:" && pass "picked from the list: $SRC -> $NAME MIDI in" || fail "picking $SRC did not connect it: $(jack_lsp -c | tr '\n' ' ')"
prefs_button $OK
dismiss_audio_box && fail "Audio box after OK: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
[ "$(jack_lsp | grep -c "^$NAME:outport ")" -ge 2 ] && pass "after JACK -> PulseAudio -> JACK and OK the audio runs again" || fail "no audio ports after OK: $(jack_lsp | tr '\n' ' ')"
[ "$(ini audio driver)" = driver=1 ] && [ "$(ini midi indev)" = "indev=$SRC" ] && pass "settings.ini after OK: $(ini audio driver) $(ini midi indev)" || fail "settings.ini after OK: $(ini audio driver) $(ini midi indev)"
M=$(meter_moves "$OUT/10-note-after-switch.png") && pass "note after the switches moves the meter ($M)" || fail "meter unchanged after the switches ($M)"

read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after File > Quit"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "File > Quit with JACK MIDI connected, exit 0" || fail "File > Quit, exit $RC"; fi
[ -z "$(app_midi_ports)" ] && pass "no $NAME MIDI ports after quit" || fail "MIDI ports after quit: $(app_midi_ports)"

# Threads alive when main() returns, with the MIDI input connected (settings.ini from the run above)
cat > "$OUT/exit-threads.gdb" <<'EOF'
set pagination off
break exit
run
info threads
thread apply all bt 3
continue
EOF
/usr/bin/gdb -q -batch -x "$OUT/exit-threads.gdb" "$APP" > "$OUT/exit-threads.txt" 2>&1 &
GPID=$!
sleep 8
connected "$SRC" "$NAME MIDI in:" && pass "under gdb: $SRC -> $NAME MIDI in" || fail "under gdb: $SRC not connected"
read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 100); do kill -0 "$GPID" 2>/dev/null || break; sleep 0.1; done
kill -0 "$GPID" 2>/dev/null && { fail "gdb run did not finish"; kill -9 "$GPID"; pkill -x "$NAME"; }
N=$(awk '/^  Id +Target Id/{f=1;next} f&&/^[* ] +[0-9]+ +Thread/{n++} END{print n+0}' "$OUT/exit-threads.txt")
awk '/^  Id +Target Id/{f=1} f&&/^[* ] +[0-9]+ +Thread/' "$OUT/exit-threads.txt" | sed 's/^/  /' | tee -a "$OUT/checks.txt"
[ "$N" -ge 1 ] && pass "$N thread(s) at exit()" || fail "no thread list at exit() (see exit-threads.txt)"
awk '/^  Id +Target Id/{f=1} f&&/^[* ] +[0-9]+ +Thread/' "$OUT/exit-threads.txt" | grep -v '"gmain"' | grep -qv '^\* *1 '  \
  && fail "a thread besides main and gmain at exit() (exit-threads.txt)" || pass "only main and gmain at exit()"

# Started before the JACK server: the Audio box, and the saved MIDI ports stay in settings.ini
kill "$SEQPID" "$DUMPPID" 2> /dev/null; jack_down
"$APP" > "$OUT/stderr-no-server.txt" 2>&1 &
APID=$!
sleep 5
dismiss_audio_box && pass "no JACK server: Audio box ($(grep -a 'Audio is off' "$OUT/stderr-no-server.txt" | tail -1))" || fail "no JACK server: no Audio box"
import -window root "$OUT/11-no-server.png"
# The server comes up later; Apply in Preferences brings back the audio and the saved MIDI ports
(jackd --no-realtime -d dummy -r 48000 -p 256 > "$OUT/jackd-late.log" 2>&1 &); sleep 1.5
jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 &
SEQPID=$!
jack_midi_dump > /dev/null 2>&1 &
DUMPPID=$!
sleep 1
prefs_open
prefs_button $APPLY
dismiss_audio_box && fail "server up, Apply: Audio box: $(grep -a 'Audio is off' "$OUT/stderr-no-server.txt" | tail -1)"
import -window root "$OUT/12-server-up-apply.png"
[ "$(jack_lsp | grep -c "^$NAME:outport ")" -ge 2 ] && pass "server up, Apply: audio runs" || fail "server up, Apply: no audio ports"
connected "$SRC" "$NAME MIDI in:" && connected "$DUMP" "$NAME MIDI out:" \
  && pass "server up, Apply: the saved MIDI ports reconnect" || fail "server up, Apply: MIDI not reconnected: $(jack_lsp -c | tr '\n' ' ')"
prefs_button $OK
M=$(meter_moves "$OUT/13-server-up-note.png") && pass "server up: the note moves the meter ($M)" || fail "server up: meter unchanged ($M)"

# The source client restarts while the APP's input is open: OK reconnects it
kill "$SEQPID"; sleep 1
jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 &
SEQPID=$!
sleep 2
connected "$SRC" "$NAME MIDI in:" && fail "source restarted: connected without OK?" || pass "source restarted: $SRC no longer connected"
# A second source connected in a patchbay: OK must not drop it
jack_midiseq seq2 48000 24000 64 2400 > /dev/null 2>&1 &
SEQ2PID=$!
sleep 1
jack_connect seq2:out "$NAME MIDI in:RtMidi Input"
prefs_open; prefs_button $OK
connected "$SRC" "$NAME MIDI in:" && pass "source restarted, OK: $SRC -> $NAME MIDI in again" || fail "source restarted, OK: not reconnected"
connected seq2:out "$NAME MIDI in:" && pass "OK keeps the patchbay connection seq2:out -> $NAME MIDI in" || fail "OK dropped the patchbay connection from seq2:out"
kill "$SEQ2PID"

# The JACK server restarts while the APP runs: the old MIDI clients died with it; OK brings back audio and MIDI
kill "$SEQPID" "$DUMPPID" 2> /dev/null; jack_down
(jackd --no-realtime -d dummy -r 48000 -p 256 > "$OUT/jackd-restart.log" 2>&1 &); sleep 1.5
jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 &
SEQPID=$!
jack_midi_dump > /dev/null 2>&1 &
DUMPPID=$!
sleep 2
dismiss_audio_box && info "server restart: Audio box ($(grep -a 'Audio is off' "$OUT/stderr-no-server.txt" | tail -1))"
prefs_open; prefs_button $OK
dismiss_audio_box && fail "server restarted, OK: Audio box: $(grep -a 'Audio is off' "$OUT/stderr-no-server.txt" | tail -1)"
[ "$(jack_lsp | grep -c "^$NAME:outport ")" -ge 2 ] && pass "server restarted, OK: audio runs" || fail "server restarted, OK: no audio ports"
connected "$SRC" "$NAME MIDI in:" && connected "$DUMP" "$NAME MIDI out:" \
  && pass "server restarted, OK: both MIDI ports reconnect" || fail "server restarted, OK: MIDI not reconnected: $(jack_lsp -c | tr '\n' ' ')"
M=$(meter_moves "$OUT/14-server-restarted-note.png") && pass "server restarted: the note moves the meter ($M)" || fail "server restarted: meter unchanged ($M)"
read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "no JACK server: APP still running 5 s after File > Quit"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "after the restarts: File > Quit, exit 0" || fail "after the restarts: File > Quit, exit $RC"; fi
[ "$(ini midi indev)" = "indev=$SRC" ] && [ "$(ini midi outdev)" = "outdev=$DUMP" ] \
  && pass "no JACK server: settings.ini keeps $(ini midi indev) $(ini midi outdev)" || fail "no JACK server: settings.ini $(ini midi indev) $(ini midi outdev)"

if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; fi
