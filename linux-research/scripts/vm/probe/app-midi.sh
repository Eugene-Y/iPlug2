#!/usr/bin/env bash
# T-004: MIDI input reaches the APP. A snd-virmidi card gives an ALSA sequencer port that amidi can play into;
# settings.ini selects it as the APP's MIDI input. Checks the sequencer connection, the instrument's meter on a
# note, the device in Preferences, and a clean quit. VM only (needs /dev/snd/seq and the module).
# Usage: vm-run.sh vm/probe/app-midi.sh <out> <IPlugInstrument APP>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"; : > "$OUT/checks.txt"
FAILS=0
fail() { echo "FAIL: $*" | tee -a "$OUT/checks.txt"; FAILS=$((FAILS + 1)); }
pass() { echo "ok: $*" | tee -a "$OUT/checks.txt"; }
win() { local w; for w in $(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null); do
  xprop -id "$w" WM_CLASS 2>/dev/null | grep -q "\"$NAME\"" && { echo "$w"; return 0; }; done; }
origin() { xwininfo -id "$1" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}'; }
slow_click() { xdotool mousemove "$1" "$2" mousedown 1; sleep 0.2; xdotool mouseup 1; sleep 0.8; }
meter_pixel() { local X Y; read -r X Y < <(origin "$(win "$NAME")"); import -window root -crop "1x1+$((X + 943))+$((Y + 294))" txt:- | tail -1 | awk '{print $3}'; }

aconnect -i | grep -q "Virtual Raw MIDI" || sudo -n modprobe snd-virmidi midi_devs=1
sleep 1
# client 20: 'Virtual Raw MIDI 1-0' [type=kernel,card=1]
#     0 'VirMIDI 1-0     '            <- aconnect pads the port name to 16 columns
CLINE=$(aconnect -i | grep -m1 "^client [0-9]*: 'Virtual Raw MIDI")
CLIENT=$(sed -E 's/^client ([0-9]+):.*/\1/' <<<"$CLINE")
CARD=$(sed -E 's/.*card=([0-9]+).*/\1/' <<<"$CLINE")
CNAME=$(sed -E "s/^client [0-9]+: '([^']*)'.*/\1/" <<<"$CLINE")
PNAME=$(aconnect -i | grep -A1 -F "$CLINE" | tail -1 | sed -E "s/^ +0 '(.*[^ ]) *'\$/\1/")
[ -n "$CARD" ] && pass "virmidi: seq client $CLIENT '$CNAME' port '$PNAME', rawmidi hw:$CARD,0" || { fail "no VirMIDI sequencer client: $(aconnect -i)"; exit 1; }
PORT="$CNAME:$PNAME $CLIENT:0"

export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME/.config/$NAME"
# Only the settings files are evidence; Mesa's shader cache and PulseAudio's auth cookie are not
tidy_home() { rm -rf "$HOME/.cache" "$HOME/.config/pulse"; }
trap tidy_home EXIT
printf '[audio]\ndriver=2\n[midi]\nindev=%s\noutdev=off\ninchan=0\n' "$PORT" > "$HOME/.config/$NAME/settings.ini"

"$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
aconnect -l > "$OUT/aconnect.txt"
grep -A3 "^client $CLIENT:" "$OUT/aconnect.txt" | grep -q "Connecting To" \
  && pass "sequencer: $(grep -A3 "^client $CLIENT:" "$OUT/aconnect.txt" | grep 'Connecting To' | tr -s ' ')" \
  || fail "VirMIDI not connected to the APP (aconnect.txt)"
grep -qxF "indev=$PORT" "$HOME/.config/$NAME/settings.ini" && pass "settings keep indev=$PORT" || fail "indev reset: $(grep indev "$HOME/.config/$NAME/settings.ini")"

QUIET=$(meter_pixel)
amidi -p "hw:$CARD,0" -S '90 3C 7F'
LOUD=""; for _ in 1 2 3 4 5; do sleep 0.2; LOUD=$(meter_pixel); [ "$LOUD" != "$QUIET" ] && break; done
import -window root "$OUT/01-midi-note.png"
amidi -p "hw:$CARD,0" -S '80 3C 00'
[ "$LOUD" != "$QUIET" ] && pass "note on from amidi moves the meter ($QUIET -> $LOUD)" || fail "meter unchanged ($QUIET) on a MIDI note"

read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 29)); sleep 0.5
P=$(win 'Preferences.*')
if [ -n "$P" ]; then
  import -window root "$OUT/02-prefs.png"
  read -r PX PY < <(origin "$P"); slow_click $((PX + 121)) $((PY + 427)); import -window root "$OUT/03-prefs-midi-inputs.png"
  xdotool key Escape; sleep 0.5; xdotool windowactivate --sync "$P"; xdotool key Escape; sleep 0.8
fi

read -r X Y < <(origin "$(win "$NAME")")
slow_click $((X + 16)) $((Y + 6)); slow_click $((X + 41)) $((Y + 49))
for _ in $(seq 50); do kill -0 "$APID" 2>/dev/null || break; sleep 0.1; done
if kill -0 "$APID" 2>/dev/null; then fail "APP still running 5 s after File > Quit"; kill -9 "$APID"
else wait "$APID"; RC=$?; [ "$RC" = 0 ] && pass "File > Quit, exit 0" || fail "File > Quit, exit $RC"; fi
aconnect -l | grep -q "^client [0-9]*: '$NAME'" && fail "the APP's sequencer client is left" || pass "no $NAME sequencer client left"

if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; fi
