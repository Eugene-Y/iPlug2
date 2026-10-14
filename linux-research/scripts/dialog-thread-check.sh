#!/usr/bin/env bash
# gdb: which threads run the dialog code while a save dialog shows and when it is cancelled (tid=1 is REAPER's main
# thread). Runs INSIDE the GUI container / on the VM. Usage: dialog-thread-check.sh <outdir> "<FMT: Name>" <plugin>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
read -r _ OX OY < <("$HERE/dialog-up.sh" "$FX" "$PLUG")
RPID=$(cat /tmp/reaper.pid)
( sleep 4; xdotool mousemove $((OX + 889)) $((OY + 628)) click 1; sleep 4
  w=$(xdotool search --onlyvisible --name '^Save File$' | tail -1); xdotool windowactivate --sync "$w"; xdotool key Escape ) &
"$HERE/gdb-count.sh" "$RPID" 14 'iplug::igraphics::LinuxDialogs::Show' 'iplug::igraphics::LinuxDialogs::Poll' \
  'iplug::LinuxBackgroundProcess::Start' 'iplug::LinuxBackgroundProcess::Pump' | tee "$OUT/dialog-threads.txt"
wait
grep -a '^IPLUG_TEST_DIALOG' /tmp/reaper.log | tee -a "$OUT/dialog-threads.txt"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
