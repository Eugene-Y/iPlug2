#!/usr/bin/env bash
# T-004 probe: start the APP on JACK through PipeWire with fresh settings; list its JACK ports; dump stacks.
# Usage: vm-run.sh vm/probe/app-jack-start.sh <out> <app>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
export HOME="$OUT/home"; rm -rf "$HOME"; mkdir -p "$HOME/.config/$NAME"
printf '[audio]\ndriver=1\nsr=48000\nbuffer=256\n' > "$HOME/.config/$NAME/settings.ini"
JACKLIB=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack
LD_LIBRARY_PATH=$JACKLIB "$APP" > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 6
LD_LIBRARY_PATH=$JACKLIB jack_lsp | grep -i "$NAME" > "$OUT/jack-ports.txt"
echo "ports: $(tr '\n' ' ' < "$OUT/jack-ports.txt")"
sudo -n /usr/bin/gdb -q -batch -p "$APID" -ex "info threads" -ex "thread apply all bt 8" > "$OUT/bt.txt" 2>&1
kill -TERM "$APID"; sleep 3; kill -0 "$APID" 2>/dev/null && { echo "still running after SIGTERM"; kill -9 "$APID"; } || echo "exited"
rm -rf "$HOME/.cache" "$HOME/.config/pulse"
