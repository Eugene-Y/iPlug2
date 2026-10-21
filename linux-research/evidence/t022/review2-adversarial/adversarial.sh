#!/usr/bin/env bash
# T-022 review: adversarial runs of the APP's JACK MIDI, inside the GUI container (no jackd running at start).
# A: JACK in settings.ini but no JACK server; the server comes up later.  B: --no-io.  C: MIDI output off while
# IPlugInstrument echoes incoming notes, then an output picked.  D: JACK <-> ALSA toggled 10 times while notes flow.
# C2: output on -> off for 30 s while notes are echoed -> on again; then the output must still deliver.
# E: MIDI ports off, JACK -> ALSA -> JACK in the combo, then Cancel: does audio come back?
# Usage: adversarial.sh <outdir> <IPlugInstrument APP> [scenarios, default "A B C D"; "Cgdb" runs C under gdb]
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP"); RUN=" ${3:-A B C D} "
want() { [[ "$RUN" == *" $1 "* ]]; }
export LC_ALL=C.UTF-8
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; touch "$OUT/log.txt"
log() { echo "$*" | tee -a "$OUT/log.txt"; }
SRC=seq:out; DUMP=midi-monitor:input
export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME/.config/$NAME"
INI="$HOME/.config/$NAME/settings.ini"
write_ini() { printf '[audio]\ndriver=1\nindev=system\noutdev=system\nsr=48000\nbuffer=256\n[midi]\nindev=%s\noutdev=%s\ninchan=0\noutchan=0\n' "$1" "$2" > "$INI"; }
ini() { awk -F= -v s="[$1]" -v k="$2" '$0==s{f=1;next} /^\[/{f=0} f&&$1==k{print; exit}' "$INI"; }
win() { local w; for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }; done; }
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
dismiss_box() { local b; b=$(win "${1:-Audio}"); [ -n "$b" ] && { xdotool windowactivate --sync "$b"; xdotool key Return; sleep 0.8; }; [ -n "$b" ]; }
app_midi_ports() { jack_lsp 2>/dev/null | grep "^$NAME MIDI " | tr '\n' ' '; }
connected() { jack_lsp -c 2>/dev/null | awk -v p="$1" -v q="$2" '$0==p{f=1;next} /^[^ \t]/{f=0} f&&index($0,q){found=1} END{exit !found}'; }
prefs_open() { read -r X Y < <(origin "$(win "$NAME")"); slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5; P=$(win 'Preferences.*'); }
prefs_driver() { read -r X Y < <(origin "$P"); slow_click $((X + 131)) $((Y + 75)); slow_click $((X + 83)) $((Y + 96 + 16 * $1)); }
prefs_button() { read -r X Y < <(origin "$P"); slow_click $((X + $1)) $((Y + 553)); sleep 1; }
# A restarted jack_midiseq is a new port: the APP's earlier connection to it is gone
restart_source() { jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 & sleep 0.5; jack_connect "$SRC" "$NAME MIDI in:RtMidi Input"; }
APPLY=150; OK=256; CANCEL=362; MIDI_IN=427; MIDI_OUT=484
midi_list() { read -r X Y < <(origin "$P"); slow_click $((X + 121)) $((Y + $1)); import -window root "$2"
  if [ -n "${3:-}" ]; then slow_click $((X + 60)) $((Y + $1 + 21 + 16 * $3)); else xdotool key Escape; sleep 0.5; fi; }
quit_app() { # <pid> <label>
  local X Y; read -r X Y < <(origin "$(win "$NAME")"); slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
  for _ in $(seq 80); do kill -0 "$1" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$1" 2>/dev/null; then log "$2: STILL RUNNING 8 s after File > Quit"; kill -9 "$1"; else wait "$1"; log "$2: File > Quit exit $?"; fi; }
alive() { kill -0 "$1" 2>/dev/null && log "$2: APP alive" || { wait "$1"; log "$2: APP DIED, exit $?"; }; }
jack_up() { (jackd --no-realtime -d dummy -r 48000 -p 256 > "$OUT/jackd-$1.log" 2>&1 &); sleep 1.5; }
jack_down() { pkill -x jack_midiseq; pkill -x jack_midi_dump; pkill -x jackd; for _ in $(seq 150); do pgrep -x jackd > /dev/null || break; sleep 0.1; done; pkill -9 -x jackd; sleep 0.5; }
pkill -x "$NAME"; jack_down

if want A; then
log "=== A: JACK selected, no JACK server"
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/A-stderr.txt" 2>&1 & APID=$!
sleep 5
dismiss_box && log "A: Audio box at start (expected): $(grep -a 'Audio is off' "$OUT/A-stderr.txt" | tail -1)"
alive $APID A
log "A: RtMidi warnings in stderr: $(grep -ac 'JACK server not running' "$OUT/A-stderr.txt")"
log "A: settings.ini after start: $(ini midi indev) $(ini midi outdev)"
prefs_open; log "A: Preferences '$(xdotool getwindowname "$P" 2>/dev/null)'"
midi_list $MIDI_IN "$OUT/A1-no-server-midi-inputs.png"
jack_up A
jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 &
log "A: server up; Apply on JACK (same MIDI API, no re-probe)"
prefs_button $APPLY; dismiss_box && log "A: Audio box after Apply"
midi_list $MIDI_IN "$OUT/A2-server-up-same-driver-inputs.png"
log "A: APP MIDI ports now: $(app_midi_ports)"
prefs_driver 0; prefs_driver 1
midi_list $MIDI_IN "$OUT/A3-after-alsa-jack-inputs.png" 1
connected "$SRC" "$NAME MIDI in:" && log "A: picked after ALSA->JACK: $SRC -> $NAME MIDI in" || log "A: picking did not connect: $(jack_lsp -c | tr '\n' ' ')"
prefs_button $OK; dismiss_box && log "A: Audio box after OK"
alive $APID A
log "A: APP JACK ports: $(jack_lsp | grep "^$NAME" | tr '\n' ' ')"
quit_app $APID A
log "A: RtMidi warnings in stderr at the end: $(grep -ac 'JACK server not running' "$OUT/A-stderr.txt")"
jack_down

fi
if want B; then
log "=== B: --no-io, JACK in settings.ini, server running"
jack_up B
write_ini "$SRC" "$DUMP"
"$APP" --no-io > "$OUT/B-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "B: Audio box"
alive $APID B
log "B: APP JACK ports: '$(jack_lsp | grep "^$NAME" | tr '\n' ' ')'"
log "B: settings.ini after start: $(ini midi indev) $(ini midi outdev)"
prefs_open; log "B: Preferences '$(xdotool getwindowname "$P" 2>/dev/null)'"
prefs_driver 0; prefs_driver 2; prefs_driver 1
import -window root "$OUT/B1-no-io-after-switches.png"
log "B: APP JACK ports after switches: '$(jack_lsp | grep "^$NAME" | tr '\n' ' ')'"
prefs_button $CANCEL; dismiss_box && log "B: Audio box after Cancel"
alive $APID B
log "B: settings.ini after Cancel: $(ini audio driver) $(ini midi indev) $(ini midi outdev)"
quit_app $APID B
jack_down

fi
if want C || want Cgdb; then
log "=== C: MIDI output off while the instrument echoes notes; then an output picked"
jack_up C
# a note on and off every 10 ms: 200 echoed messages a second into MidiOutJack's ring buffer, which nothing drains
jack_midiseq seq 480 0 60 240 > /dev/null 2>&1 &
jack_midi_dump > "$OUT/C-midi-out.txt" 2> /dev/null &
write_ini "$SRC" off
if want Cgdb; then
  printf 'set pagination off\nhandle SIGPIPE nostop noprint\nrun\ninfo threads\nbt 12\nthread apply all bt 4\n' > "$OUT/C-crash.gdb"
  /usr/bin/gdb -q -batch -x "$OUT/C-crash.gdb" "$APP" > "$OUT/C-gdb.txt" 2>&1 & APID=$!
else
  "$APP" > "$OUT/C-stderr.txt" 2>&1 & APID=$!
fi
sleep 5; dismiss_box && log "C: Audio box"
connected "$SRC" "$NAME MIDI in:" && log "C: $SRC -> $NAME MIDI in" || log "C: input not connected"
log "C: waiting 30 s with the MIDI output off"; sleep 30
pkill -x jack_midiseq; sleep 1
alive $APID C
prefs_open
midi_list $MIDI_OUT "$OUT/C1-outputs.png" 1
sleep 2
alive $APID C
connected "$DUMP" "$NAME MIDI out:" && log "C: $NAME MIDI out -> $DUMP" || log "C: output not connected"
log "C: events at $DUMP after the output was picked, with the source stopped: $(grep -c 'note' "$OUT/C-midi-out.txt")"
log "C: distinct timestamps column values (one burst => events share a cycle): $(awk '{print $1}' "$OUT/C-midi-out.txt" | sort | uniq -c | sort -rn | head -3 | tr '\n' ' ')"
grep -a -i 'error\|full\|limit' "$OUT/C-stderr.txt" | tail -3 | sed 's/^/C stderr: /' | tee -a "$OUT/log.txt"
N0=$(grep -c note "$OUT/C-midi-out.txt")
restart_source
sleep 3.5; pkill -x jack_midiseq; sleep 0.5
log "C: after restarting the source for 3.5 s (a note a second): $(( $(grep -c note "$OUT/C-midi-out.txt") - N0 )) events at $DUMP (expect about 6-8)"
prefs_button $CANCEL
if kill -0 $APID 2>/dev/null; then quit_app $APID C; fi
jack_down
fi
if want C2; then
log "=== C2: output connected -> off for 30 s while notes are echoed -> connected again"
jack_up C2
jack_midiseq seq 480 0 60 240 > /dev/null 2>&1 &
jack_midi_dump > "$OUT/C2-midi-out.txt" 2> /dev/null &
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/C2-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "C2: Audio box"
log "C2: echoed events with the output connected (5 s): $(grep -c note "$OUT/C2-midi-out.txt")"
prefs_open; midi_list $MIDI_OUT "$OUT/C2-1-off.png" 0; prefs_button $OK
N0=$(grep -c note "$OUT/C2-midi-out.txt")
log "C2: output off, waiting 30 s"; sleep 30
log "C2: events at $DUMP during the 30 s off: $(( $(grep -c note "$OUT/C2-midi-out.txt") - N0 ))"
pkill -x jack_midiseq; sleep 1
N1=$(grep -c note "$OUT/C2-midi-out.txt")
prefs_open; midi_list $MIDI_OUT "$OUT/C2-2-on-again.png" 1; prefs_button $OK
sleep 2; alive $APID C2
log "C2: events at $DUMP right after reconnecting, source stopped: $(( $(grep -c note "$OUT/C2-midi-out.txt") - N1 ))"
restart_source
N2=$(grep -c note "$OUT/C2-midi-out.txt"); sleep 3.5; pkill -x jack_midiseq; sleep 0.5
log "C2: after restarting the source for 3.5 s: $(( $(grep -c note "$OUT/C2-midi-out.txt") - N2 )) events (expect about 6-8)"
if kill -0 $APID 2>/dev/null; then quit_app $APID C2; fi
jack_down
fi
if want D; then
log "=== D: JACK <-> ALSA x10 in the driver combo while notes flow"
jack_up D
jack_midiseq seq 480 0 60 240 > /dev/null 2>&1 &
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/D-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "D: Audio box"
prefs_open
for i in $(seq 10); do prefs_driver 0; prefs_driver 1; done
alive $APID D
midi_list $MIDI_IN "$OUT/D1-inputs-after-10-toggles.png"
prefs_button $OK; dismiss_box && log "D: Audio box after OK"
connected "$SRC" "$NAME MIDI in:" && log "D: $SRC -> $NAME MIDI in" || log "D: input not connected after toggles (saved port turned off by the ALSA step): $(ini midi indev)"
log "D: APP JACK clients: $(jack_lsp | grep "^$NAME" | cut -d: -f1 | sort -u | tr '\n' '|')"
quit_app $APID D
jack_down
fi
if want E; then
log "=== E: MIDI ports off; JACK -> ALSA -> JACK in the combo, then Cancel"
jack_up E
write_ini off off
"$APP" > "$OUT/E-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "E: Audio box"
log "E: audio ports before: $(jack_lsp | grep -c "^$NAME:outport")"
prefs_open; prefs_driver 0; prefs_driver 1
log "E: audio ports with Preferences open after the switches: $(jack_lsp | grep -c "^$NAME:outport")"
prefs_button $CANCEL; dismiss_box && log "E: Audio box after Cancel"
sleep 1
log "E: audio ports after Cancel: $(jack_lsp | grep -c "^$NAME:outport")"
quit_app $APID E
jack_down
fi
rm -rf "$HOME/.cache" "$HOME/.config/pulse"
