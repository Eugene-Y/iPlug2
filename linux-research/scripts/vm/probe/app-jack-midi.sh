#!/usr/bin/env bash
# T-022: the APP's MIDI follows its audio driver on a PipeWire desktop. A snd-virmidi card is both an ALSA sequencer
# port and, through PipeWire's Midi-Bridge, a JACK MIDI port; amidi plays a note into it. JACK (pipewire-jack's
# libjack): the APP's JACK MIDI input from the bridge, the echo at jack_midi_dump. PulseAudio: the MIDI list shows the
# sequencer's ports, one picked there plays. JACK again: the JACK port picked from the list plays. Then File > Quit
# on JACK; then a run under gdb on PulseAudio with the sequencer input open, File > Quit and the threads left at
# exit(). VM only (needs /dev/snd/seq and the module).
# Usage: vm-run.sh vm/probe/app-jack-midi.sh <out> <IPlugInstrument APP>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
export LC_ALL=C.UTF-8
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; : > "$OUT/checks.txt"
FAILS=0
fail() { echo "FAIL: $*" | tee -a "$OUT/checks.txt"; FAILS=$((FAILS + 1)); }
pass() { echo "ok: $*" | tee -a "$OUT/checks.txt"; }
info() { echo "info: $*" | tee -a "$OUT/checks.txt"; }
JACKLIB="LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack"
jack() { env "$JACKLIB" "$@" 2>/dev/null; }
pkill -x "$NAME"; pkill -x jack_midi_dump; sleep 0.5

aconnect -i | grep -q "Virtual Raw MIDI" || sudo -n modprobe snd-virmidi midi_devs=1
sleep 2
CLINE=$(aconnect -i | grep -m1 "^client [0-9]*: 'Virtual Raw MIDI")
CARD=$(sed -E 's/.*card=([0-9]+).*/\1/' <<<"$CLINE")
[ -n "$CARD" ] && pass "virmidi: $CLINE, rawmidi hw:$CARD,0" || { fail "no VirMIDI sequencer client: $(aconnect -i)"; exit 1; }
SRC=$(jack jack_lsp | grep -m1 "^Midi-Bridge:Virtual Raw MIDI.*(capture_0)")
[ -n "$SRC" ] && pass "JACK port of the virmidi card: '$SRC'" || { fail "no Midi-Bridge port for virmidi: $(jack jack_lsp | tr '\n' ' ')"; exit 1; }
DUMP=midi-monitor:input
jack jack_midi_dump > "$OUT/midi-out.txt" &
DUMPPID=$!
sleep 1

export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME/.config/$NAME"
# Only the settings files are evidence; Mesa's shader cache and PulseAudio's auth cookie are not
cleanup() { rm -rf "$HOME/.cache" "$HOME/.config/pulse"; kill "$DUMPPID" 2> /dev/null; }
trap cleanup EXIT
INI="$HOME/.config/$NAME/settings.ini"
printf '[audio]\ndriver=1\nindev=system\noutdev=system\nsr=48000\nbuffer=256\n[midi]\nindev=%s\noutdev=%s\ninchan=0\noutchan=0\n' \
  "$SRC" "$DUMP" > "$INI"

win() { local w; for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do
  xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }; done; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
dismiss_audio_box() { local b; b=$(win Audio); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Return; sleep 0.8; }; [ -n "$b" ]; }
app_jack_midi_ports() { jack jack_lsp | grep "^$NAME MIDI " | tr '\n' ' '; }
# jack_lsp -c lists each port, then its connections indented
jack_connected() { jack jack_lsp -c | awk -v p="$1" -v q="$2" '$0==p{f=1;next} /^[^ \t]/{f=0} f&&index($0,q){found=1} END{exit !found}'; }
SEQCLIENT=$(sed -E 's/^client ([0-9]+):.*/\1/' <<<"$CLINE")
meter_pixel() { local X Y; read -r X Y < <(origin "$(win "$NAME")"); import -window root -crop "1x1+$((X + 943))+$((Y + 294))" txt:- | tail -1 | awk '{print $3}'; }
note_moves_meter() {
  local quiet px
  quiet=$(meter_pixel)
  amidi -p "hw:$CARD,0" -S '90 3C 7F'
  for _ in 1 2 3 4 5; do sleep 0.2; px=$(meter_pixel); [ "$px" != "$quiet" ] && break; done
  [ -n "${1:-}" ] && import -window root "$1"
  amidi -p "hw:$CARD,0" -S '80 3C 00'
  echo "$quiet -> $px"; [ "$px" != "$quiet" ]
}
prefs_open() {
  read -r X Y < <(origin "$(win "$NAME")")
  slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5
  P=$(win 'Preferences.*')
}
# GNOME's list rows: the first 23 px below the combo, then 18 px apart
prefs_driver() { read -r X Y < <(origin "$P"); slow_click $((X + 131)) $((Y + 75)); slow_click $((X + 83)) $((Y + 98 + 18 * $1)); }
prefs_button() { read -r X Y < <(origin "$P"); slow_click $((X + $1)) $((Y + 553)); sleep 1; }
APPLY=150; OK=256
MIDI_IN=427
midi_list() { # <combo y> <png> [row to pick; Escape otherwise]
  read -r X Y < <(origin "$P")
  slow_click $((X + 121)) $((Y + $1)); import -window root "$2"
  if [ -n "${3:-}" ]; then slow_click $((X + 60)) $((Y + $1 + 23 + 18 * $3)); else xdotool key Escape; sleep 0.5; fi
}
ini() { awk -F= -v s="[$1]" -v k="$2" '$0==s{f=1;next} /^\[/{f=0} f&&$1==k{print; exit}' "$INI"; }

env "$JACKLIB" "$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
dismiss_audio_box && fail "Audio box at start on JACK: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
info "the APP's JACK MIDI ports: $(app_jack_midi_ports)"
jack_connected "$SRC" "$NAME MIDI in:" && pass "JACK: '$SRC' -> $NAME MIDI in" || fail "JACK: '$SRC' not connected to the APP"
jack_connected "$DUMP" "$NAME MIDI out:" && pass "JACK: $NAME MIDI out -> $DUMP" || fail "JACK: the APP's MIDI output not connected to $DUMP"
aconnect -l | grep -q "^client [0-9]*: '$NAME" && fail "an ALSA sequencer client on JACK: $(aconnect -l | grep "'$NAME")" || pass "no ALSA sequencer client on JACK"
M=$(note_moves_meter "$OUT/01-jack-note.png") && pass "JACK: amidi note moves the meter ($M)" || fail "JACK: meter unchanged ($M)"
sleep 0.5
grep -q ' 90 3c 7f' "$OUT/midi-out.txt" && pass "JACK: the note echoed to $DUMP: $(grep -m1 ' 90 3c' "$OUT/midi-out.txt")" || fail "JACK: nothing echoed to $DUMP"

prefs_open
[ -n "$P" ] && pass "Preferences '$(xdotool getwindowname "$P")'" || fail "Preferences did not open"
midi_list $MIDI_IN "$OUT/02-jack-midi-inputs.png"

prefs_driver 2
[ -z "$(app_jack_midi_ports)" ] && pass "PulseAudio: no JACK MIDI ports left" || fail "PulseAudio: JACK MIDI ports left: $(app_jack_midi_ports)"
# Sequencer ports in client order: off, Midi Through (14), the virmidi card
midi_list $MIDI_IN "$OUT/03-pulse-midi-inputs.png" 2
# aconnect lists a sequencer client only once it has a port
TO=$(aconnect -l | grep -A3 "^client $SEQCLIENT:" | sed -nE 's/.*Connecting To: ([0-9]+):.*/\1/p' | head -1)
TONAME=$(aconnect -l | grep -m1 "^client $TO:" | sed -E "s/^client [0-9]+: '([^']*)'.*/\1/")
[ "$TONAME" = "$NAME" ] && pass "PulseAudio: picked from the list, virmidi -> sequencer client $TO '$TONAME'" \
  || fail "PulseAudio: virmidi not connected to '$NAME' after picking it ($(aconnect -l | tr '\n' ' '))"
prefs_button $OK
dismiss_audio_box && fail "Audio box on PulseAudio: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
SEQ_IN=$(ini midi indev); SEQ_IN=${SEQ_IN#indev=}
info "settings.ini on PulseAudio: $(ini audio driver) indev=$SEQ_IN"
M=$(note_moves_meter "$OUT/04-pulse-note.png") && pass "PulseAudio: amidi note through the sequencer moves the meter ($M)" || fail "PulseAudio: meter unchanged ($M)"

prefs_open
prefs_driver 1
aconnect -l | grep -q "^client [0-9]*: '$NAME'" && fail "JACK: sequencer client left" || pass "JACK again: no sequencer client left"
# JACK's MIDI outputs in jack_get_ports order after "off"
ROW=$(jack jack_lsp -p -t | awk -v s="$SRC" '/^[^ \t]/{n=$0;next} /properties:/{o=($0~/output/)} /midi/&&o{i++; if(n==s){print i; exit}}')
info "JACK again: '$SRC' is row $ROW of the MIDI input list"
midi_list $MIDI_IN "$OUT/05-jack-again-midi-inputs.png" "$ROW"
jack_connected "$SRC" "$NAME MIDI in:" && pass "JACK again: picked from the list, '$SRC' -> $NAME MIDI in" || fail "JACK again: '$SRC' not connected after picking it"
prefs_button $OK
dismiss_audio_box && fail "Audio box back on JACK: $(grep -a 'Audio is off' "$OUT/stderr.txt" | tail -1)"
[ "$(ini audio driver)" = driver=1 ] && [ "$(ini midi indev)" = "indev=$SRC" ] && pass "settings.ini: $(ini audio driver) $(ini midi indev)" || fail "settings.ini: $(ini audio driver) $(ini midi indev)"
M=$(note_moves_meter "$OUT/06-jack-again-note.png") && pass "JACK again: amidi note moves the meter ($M)" || fail "JACK again: meter unchanged ($M)"

read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after File > Quit"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "File > Quit on JACK with JACK MIDI connected, exit 0" || fail "File > Quit, exit $RC"; fi
[ -z "$(app_jack_midi_ports)" ] && pass "no JACK MIDI ports after quit" || fail "JACK MIDI ports after quit: $(app_jack_midi_ports)"

# The quit above was on JACK; this one is on PulseAudio with the sequencer input from the list.
# /usr/bin/gdb, not the VM's sudo wrapper: the APP must run as the desktop user to reach PipeWire
cat > "$OUT/exit-threads.gdb" <<'EOF'
set pagination off
break exit
run
info threads
thread apply all bt 3
continue
EOF
printf '[audio]\ndriver=2\nsr=48000\nbuffer=256\n[midi]\nindev=%s\noutdev=off\ninchan=0\noutchan=0\n' "$SEQ_IN" > "$INI"
/usr/bin/gdb -q -batch -x "$OUT/exit-threads.gdb" "$APP" > "$OUT/exit-threads.txt" 2>&1 &
GPID=$!
sleep 8
TO=$(aconnect -l | grep -A3 "^client $SEQCLIENT:" | sed -nE 's/.*Connecting To: ([0-9]+):.*/\1/p' | head -1)
TONAME=$(aconnect -l | grep -m1 "^client $TO:" | sed -E "s/^client [0-9]+: '([^']*)'.*/\1/")
[ "$TONAME" = "$NAME" ] && pass "under gdb on PulseAudio: virmidi -> sequencer client $TO '$TONAME'" || fail "under gdb on PulseAudio: virmidi not connected ($(aconnect -l | tr '\n' ' '))"
M=$(note_moves_meter) && pass "under gdb on PulseAudio: amidi note moves the meter ($M)" || fail "under gdb on PulseAudio: meter unchanged ($M)"
read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 100); do kill -0 "$GPID" 2>/dev/null || break; sleep 0.1; done
kill -0 "$GPID" 2>/dev/null && { fail "gdb run did not finish"; kill -9 "$GPID"; pkill -x "$NAME"; }
THREADS=$(awk '/^  Id +Target Id/{f=1;next} f&&/^[* ] +[0-9]+ +Thread/' "$OUT/exit-threads.txt")
sed 's/^/  /' <<<"$THREADS" | tee -a "$OUT/checks.txt"
[ -n "$THREADS" ] && pass "$(wc -l <<<"$THREADS" | tr -d ' ') thread(s) at exit()" || fail "no thread list at exit() (see exit-threads.txt)"
grep -v '"gmain"' <<<"$THREADS" | grep -qv '^\* *1 ' && fail "a thread besides main and gmain at exit()" || pass "only main and gmain at exit()"

if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; fi
