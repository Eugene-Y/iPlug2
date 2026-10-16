#!/usr/bin/env bash
# T-004 probe: run the APP for N seconds on one audio driver and count RtAudio's xrun reports.
# Usage: vm-run.sh vm/probe/app-xruns.sh <out> <app> <driver: 0 ALSA, 1 JACK, 2 PulseAudio> [seconds=10] [buffer=512]
set -uo pipefail
OUT="$1"; APP="$2"; DRIVER="$3"; SECS="${4:-10}"; BUF="${5:-512}"; NAME=$(basename "$APP")
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME/.config/$NAME"
printf '[audio]\ndriver=%s\nsr=48000\nbuffer=%s\n' "$DRIVER" "$BUF" > "$HOME/.config/$NAME/settings.ini"
env ${APP_ENV:-} "$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep "$SECS"
command -v pw-top >/dev/null && timeout 5 pw-top -b -n 3 2>&1 | grep -a "$NAME" | tail -2
kill -TERM "$APID"; wait "$APID"
echo "driver=$DRIVER buffer=$BUF ${SECS}s: $(grep -aci 'underrun\|overrun\|xrun' "$OUT/stderr.txt") xrun reports; $(grep -a 'Audio is off' "$OUT/stderr.txt")"
grep -a "synchron" "$OUT/stderr.txt" | head -1
