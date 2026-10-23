#!/usr/bin/env bash
# T-022 review round 3 re-check (R3 fixes); round 3 script plus K2, L, N.
# T-022 review round 3 (R1-2 follow-up): adversarial runs of the kept MIDI ports, inside the GUI container (no jackd
# running at start). Helpers as in review2-adversarial/adversarial.sh.
# F: the source port vanishes and comes back while the APP's input is open; Apply/OK.
# G: the JACK server restarts while the APP runs; Apply.
# H: a kept port (no server, combo shows "off") is cleared by picking "off"; the server comes up; Apply.
# I: audio off for lack of a device: Audio boxes on OK, Cancel, Apply (no loop, one per press).
# J: Preferences opened before the server starts; server and source up; Apply after a 2 s wait (r12 A without the race).
# Ggdb: G under gdb, for the crash at quit.
# Gq: G without Preferences: the server restarts, then File > Quit (is the crash independent of Apply/OK?).
# Gs: G, but in Preferences the driver goes JACK -> ALSA -> JACK before OK (the dead MIDI clients are destroyed before
#     the new audio client opens): does MIDI come back, and does Quit still crash?
# Go: G with OK only (no Apply): OK now restarts the stopped audio too.
# K2: K, then the source comes back and OK reselects it: the note reaches the monitor.
# L: a connection made outside the APP (jack_connect seq2:out -> the APP's input): does it survive Apply and OK?
# N: ports connected; JACK -> PulseAudio -> JACK in the combo, then Cancel: MIDI and audio back, Quit exit 0.
# K: saved output missing while notes are echoed; the output appears; Apply while the audio runs: no backlog, it works.
# Usage: adversarial3.sh <outdir> <IPlugInstrument APP> [scenarios, default "F G H I J K"]
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP"); RUN=" ${3:-F G H I J K} "
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
count_boxes() { local n=0; sleep 0.5; while dismiss_box; do n=$((n + 1)); [ $n -ge 5 ] && break; sleep 0.5; done; echo $n; }
app_midi_ports() { jack_lsp 2>/dev/null | grep "^$NAME MIDI " | tr '\n' ' '; }
audio_ports() { jack_lsp 2>/dev/null | grep -c "^$NAME:outport"; }
connected() { jack_lsp -c 2>/dev/null | awk -v p="$1" -v q="$2" '$0==p{f=1;next} /^[^ \t]/{f=0} f&&index($0,q){found=1} END{exit !found}'; }
prefs_open() { read -r X Y < <(origin "$(win "$NAME")"); slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5; P=$(win 'Preferences.*'); }
prefs_driver() { read -r X Y < <(origin "$P"); slow_click $((X + 131)) $((Y + 75)); slow_click $((X + 83)) $((Y + 96 + 16 * $1)); }
prefs_button() { read -r X Y < <(origin "$P"); slow_click $((X + $1)) $((Y + 553)); sleep 1; }
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
src_up() { jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 & }
dump_up() { jack_midi_dump > "${1:-/dev/null}" 2> /dev/null & }
conn_state() { local i o; connected "$SRC" "$NAME MIDI in:" && i=in-connected || i=in-NOT-connected; connected "$DUMP" "$NAME MIDI out:" && o=out-connected || o=out-NOT-connected; echo "$i $o"; }
pkill -x "$NAME"; jack_down

if want F; then
log "=== F: the source vanishes and comes back while the APP's input is open"
jack_up F; src_up; dump_up; sleep 1
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/F-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "F: Audio box at start"
log "F: at start: $(conn_state)"
pkill -x jack_midiseq; sleep 1; src_up; sleep 1
log "F: source restarted (same port name $SRC): $(conn_state); APP MIDI ports: $(app_midi_ports)"
prefs_open; prefs_button $APPLY; dismiss_box && log "F: Audio box after Apply"
log "F: after Apply: $(conn_state)"
prefs_button $OK; dismiss_box && log "F: Audio box after OK"
log "F: after OK: $(conn_state); settings.ini: $(ini midi indev)"
alive $APID F; quit_app $APID F
jack_down
fi

if want G || want Ggdb; then
log "=== G: the JACK server restarts while the APP runs"
jack_up G1; src_up; dump_up; sleep 1
write_ini "$SRC" "$DUMP"
if want Ggdb; then
  printf 'set pagination off\nhandle SIGPIPE nostop noprint\nrun\ninfo threads\nbt 16\nthread apply all bt 6\n' > "$OUT/G-crash.gdb"
  /usr/bin/gdb -q -batch -x "$OUT/G-crash.gdb" "$APP" > "$OUT/G-gdb.txt" 2>&1 & APID=$!
else
  "$APP" > "$OUT/G-stderr.txt" 2>&1 & APID=$!
fi
sleep 5; dismiss_box && log "G: Audio box at start"
log "G: at start: $(conn_state), audio ports $(audio_ports)"
jack_down; sleep 2
log "G: server stopped; boxes: $(count_boxes)"
alive $APID G
jack_up G2; src_up; dump_up; sleep 1.5
log "G: server back: $(conn_state), audio ports $(audio_ports)"
prefs_open; log "G: Preferences '$(xdotool getwindowname "$P" 2>/dev/null)'"
prefs_button $APPLY; log "G: boxes after Apply: $(count_boxes)"
log "G: after Apply: $(conn_state), audio ports $(audio_ports), APP MIDI ports: $(app_midi_ports)"
prefs_button $OK; log "G: boxes after OK: $(count_boxes)"
log "G: after OK: $(conn_state), audio ports $(audio_ports); settings.ini $(ini midi indev) $(ini midi outdev)"
alive $APID G
kill -0 $APID 2>/dev/null && quit_app $APID G
jack_down
fi

if want Gq; then
log "=== Gq: the JACK server restarts while the APP runs; no Preferences; quit"
jack_up Gq1; src_up; dump_up; sleep 1
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/Gq-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "Gq: Audio box at start"
log "Gq: at start: $(conn_state), audio ports $(audio_ports)"
jack_down; sleep 2
jack_up Gq2; src_up; dump_up; sleep 1.5
alive $APID Gq
quit_app $APID Gq
jack_down
fi

if want Gs; then
log "=== Gs: the JACK server restarts; Preferences: JACK -> ALSA -> JACK, OK; quit"
jack_up Gs1; src_up; dump_up; sleep 1
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/Gs-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "Gs: Audio box at start"
log "Gs: at start: $(conn_state), audio ports $(audio_ports)"
jack_down; sleep 2
jack_up Gs2; src_up; dump_up; sleep 1.5
prefs_open; prefs_driver 0; log "Gs: boxes after ALSA: $(count_boxes)"; prefs_driver 1; log "Gs: boxes after JACK: $(count_boxes)"
prefs_button $OK; log "Gs: boxes after OK: $(count_boxes)"
log "Gs: after OK: $(conn_state), audio ports $(audio_ports)"
alive $APID Gs
kill -0 $APID 2>/dev/null && quit_app $APID Gs
jack_down
fi

if want Go; then
log "=== Go: the JACK server restarts; Preferences: OK only; quit"
jack_up Go1; src_up; dump_up; sleep 1
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/Go-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "Go: Audio box at start"
jack_down; sleep 2
jack_up Go2; src_up; dump_up; sleep 1.5
prefs_open; prefs_button $OK; log "Go: boxes after OK: $(count_boxes)"
log "Go: after OK: $(conn_state), audio ports $(audio_ports)"
alive $APID Go
kill -0 $APID 2>/dev/null && quit_app $APID Go
jack_down
fi

if want H; then
log "=== H: clear a kept port while the server is down"
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/H-stderr.txt" 2>&1 & APID=$!
sleep 5; log "H: boxes at start: $(count_boxes)"
log "H: settings.ini at start: $(ini midi indev) $(ini midi outdev)"
prefs_open
midi_list $MIDI_IN "$OUT/H1-kept-input-list.png" 0
midi_list $MIDI_OUT "$OUT/H2-kept-output-list.png" 0
prefs_button $OK; log "H: boxes after OK: $(count_boxes)"
log "H: settings.ini after picking off and OK: $(ini midi indev) $(ini midi outdev)"
jack_up H; src_up; dump_up; sleep 1.5
prefs_open; prefs_button $APPLY; log "H: boxes after Apply with the server up: $(count_boxes)"
log "H: after Apply: $(conn_state), audio ports $(audio_ports)"
prefs_button $OK; count_boxes > /dev/null
log "H: settings.ini at the end: $(ini midi indev) $(ini midi outdev)"
alive $APID H; quit_app $APID H
jack_down
fi

if want I; then
log "=== I: audio off for lack of a device; OK, Cancel, Apply"
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/I-stderr.txt" 2>&1 & APID=$!
sleep 5; log "I: boxes at start: $(count_boxes)"
prefs_open; prefs_button $OK; log "I: boxes after OK: $(count_boxes); Preferences open: '$(win 'Preferences.*')'"
prefs_open; prefs_button $CANCEL; log "I: boxes after Cancel: $(count_boxes); Preferences open: '$(win 'Preferences.*')'"
prefs_open; prefs_button $APPLY; log "I: boxes after Apply 1: $(count_boxes)"
prefs_button $APPLY; log "I: boxes after Apply 2: $(count_boxes)"
import -window root "$OUT/I1-after-two-applies.png"
prefs_button $CANCEL; log "I: boxes after Cancel: $(count_boxes)"
log "I: 'Audio is off' lines in stderr: $(grep -ac 'Audio is off' "$OUT/I-stderr.txt")"
log "I: settings.ini: $(ini audio driver) $(ini midi indev) $(ini midi outdev)"
alive $APID I; quit_app $APID I
fi

if want J; then
log "=== J: Preferences opened without a server; server and source up; Apply after 2 s"
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/J-stderr.txt" 2>&1 & APID=$!
sleep 5; count_boxes > /dev/null
prefs_open
jack_up J; src_up; dump_up; sleep 2
prefs_button $APPLY; log "J: boxes after Apply: $(count_boxes)"
midi_list $MIDI_IN "$OUT/J1-inputs-after-apply.png"
log "J: after Apply: $(conn_state), audio ports $(audio_ports)"
prefs_button $OK; count_boxes > /dev/null
alive $APID J; quit_app $APID J
jack_down
fi

if want K; then
log "=== K: saved output missing while notes are echoed; it appears; Apply while the audio runs"
jack_up K
jack_midiseq seq 480 0 60 240 > /dev/null 2>&1 &
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/K-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "K: Audio box at start"
log "K: at start (no monitor yet): $(conn_state), audio ports $(audio_ports)"
log "K: 20 s of echoed notes with the output missing"; sleep 20
pkill -x jack_midiseq; sleep 1
dump_up "$OUT/K-midi-out.txt"; sleep 1
prefs_open; prefs_button $APPLY; dismiss_box && log "K: Audio box after Apply"
sleep 2
log "K: after Apply: $(conn_state); events at $DUMP with the source stopped: $(grep -c note "$OUT/K-midi-out.txt")"
N0=$(grep -c note "$OUT/K-midi-out.txt")
jack_midiseq seq 48000 0 60 2400 > /dev/null 2>&1 & sleep 0.5; jack_connect "$SRC" "$NAME MIDI in:RtMidi Input"
sleep 3.5; pkill -x jack_midiseq; sleep 0.5
log "K: after restarting the source for 3.5 s: $(( $(grep -c note "$OUT/K-midi-out.txt") - N0 )) events (expect about 6-8)"
prefs_button $OK; dismiss_box && log "K: Audio box after OK"
alive $APID K; quit_app $APID K
jack_down
fi
if want K2; then
log "=== K2: saved output missing while notes are echoed; it appears; Apply; the source comes back; OK"
jack_up K2
jack_midiseq seq 480 0 60 240 > /dev/null 2>&1 &
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/K2-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "K2: Audio box at start"
log "K2: at start (no monitor yet): $(conn_state)"
sleep 20
pkill -x jack_midiseq; sleep 1
dump_up "$OUT/K2-midi-out.txt"; sleep 1
prefs_open; prefs_button $APPLY; dismiss_box && log "K2: Audio box after Apply"
sleep 2
log "K2: after Apply: $(conn_state); events with the source stopped: $(grep -c note "$OUT/K2-midi-out.txt")"
src_up; sleep 1
prefs_button $OK; dismiss_box && log "K2: Audio box after OK"
N0=$(grep -c note "$OUT/K2-midi-out.txt"); sleep 3.5
log "K2: source back, after OK: $(conn_state); $(( $(grep -c note "$OUT/K2-midi-out.txt") - N0 )) events in 3.5 s (expect 3-4 note-ons+offs)"
alive $APID K2; quit_app $APID K2
jack_down
fi

if want L; then
log "=== L: a connection made outside the APP; Apply and OK"
jack_up L; src_up; dump_up
jack_midiseq seq2 48000 0 64 2400 > /dev/null 2>&1 &
sleep 1
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/L-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "L: Audio box at start"
jack_connect seq2:out "$NAME MIDI in:RtMidi Input"
connected seq2:out "$NAME MIDI in:" && log "L: patchbay connection seq2:out -> $NAME MIDI in made" || log "L: jack_connect failed"
prefs_open; prefs_button $APPLY
connected seq2:out "$NAME MIDI in:" && log "L: after Apply: seq2:out still connected" || log "L: after Apply: seq2:out DROPPED ($(conn_state))"
jack_connect seq2:out "$NAME MIDI in:RtMidi Input"
prefs_button $OK
connected seq2:out "$NAME MIDI in:" && log "L: after OK: seq2:out still connected" || log "L: after OK: seq2:out DROPPED ($(conn_state))"
alive $APID L; quit_app $APID L
jack_down
fi

if want N; then
log "=== N: ports connected; JACK -> PulseAudio -> JACK, Cancel"
jack_up N; src_up; dump_up; sleep 1
write_ini "$SRC" "$DUMP"
"$APP" > "$OUT/N-stderr.txt" 2>&1 & APID=$!
sleep 5; dismiss_box && log "N: Audio box at start"
log "N: at start: $(conn_state), audio ports $(audio_ports)"
prefs_open; prefs_driver 2; log "N: boxes after PulseAudio: $(count_boxes)"; prefs_driver 1; log "N: boxes after JACK: $(count_boxes)"
prefs_button $CANCEL; log "N: boxes after Cancel: $(count_boxes)"
log "N: after Cancel: $(conn_state), audio ports $(audio_ports); settings.ini $(ini audio driver) $(ini midi indev) $(ini midi outdev)"
alive $APID N; quit_app $APID N
jack_down
fi
rm -rf "$HOME/.cache" "$HOME/.config/pulse"
