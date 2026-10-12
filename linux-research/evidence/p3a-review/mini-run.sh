#!/usr/bin/env bash
# Runs INSIDE a GUI container: mini-clap-host scenarios against one .clap, mouse driven with xdotool.
# Usage: mini-run.sh <host-binary> <plugin.clap> <scenario> [secs]
set -uo pipefail
HOSTBIN="$1"; PLUG="$2"; SC="$3"; SECS="${4:-4}"
wiggle() { sleep 1.5
  for w in $(xdotool search --name mini-clap-host); do
    eval "$(xdotool getwindowgeometry --shell "$w")"
    for i in $(seq 0 20); do xdotool mousemove $((X + 60 + i * 15)) $((Y + 80 + (i % 2) * 200)); sleep 0.1; done
    xdotool mousemove $((X + 300)) $((Y + 120)) mousedown 1 mousemove $((X + 300)) $((Y + 140)) mousemove $((X + 300)) $((Y + 160)) mouseup 1
  done; }
case "$SC" in fd|fd-only|two) wiggle & ;; esac
${WRAP:-} "$HOSTBIN" "$PLUG" "$SC" "$SECS" 2>&1
echo "EXIT=$?"
wait
