#!/usr/bin/env bash
# Is the harness REAPER's stderr complete while it runs? A Sand text entry, then the trace's line counts before and
# after gdb makes REAPER flush its stdio buffers. Usage: stderr-flush-check.sh <Sand.clap> <x,y of a value, view-relative>
P="$1"; XY="${2:-501,45}"
S="$(cd "$(dirname "$0")/../../.." && pwd)"
O=$(bash "$S/vm/probe/t013/reaper-open.sh" "$P" "CLAP: Sand" 200,60 | grep '^fxwin')
read -r _ _ _ VX VY _ <<<"$O"
[ -n "$VX" ] || { echo "no view: $O"; exit 1; }
xdotool mousemove $((VX+${XY%,*})) $((VY+${XY#*,})); sleep 0.3; xdotool mousedown 1; sleep 0.08; xdotool mouseup 1
sleep 0.8; xdotool type --delay 90 2000; xdotool key Return; sleep 1
count() { echo "$1: input $(grep -c 'iPlug2 input' /tmp/reaper.log) keycode $(grep -c keycode /tmp/reaper.log) bytes $(wc -c < /tmp/reaper.log)"; }
count live
gdb -p "$(cat /tmp/reaper.pid)" -batch -ex 'call (int)fflush(0)' >/dev/null 2>&1
count "after fflush"
