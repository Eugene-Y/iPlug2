#!/usr/bin/env bash
# Start a dummy jackd (unless one runs) and Qtractor (unless it runs) with stderr = iPlug's traces, then list its
# windows. Usage: launch.sh [log] [session.qtr]
LOG=${1:-$HOME/t013-qtractor.log}
pgrep -x jackd >/dev/null || { setsid nohup jackd --no-realtime -d dummy -r 48000 -p 256 >/tmp/jackd.log 2>&1 < /dev/null & sleep 1.5; }
pgrep -x qtractor >/dev/null || { setsid nohup qtractor ${2:+"$2"} > "$LOG" 2>&1 < /dev/null & }
for i in $(seq 1 ${WAIT:-30}); do
  sleep 1
  xdotool search --onlyvisible --class qtractor >/dev/null 2>&1 && break
done
sleep 2
for w in $(xdotool search --onlyvisible --class qtractor 2>/dev/null); do
  echo "$w $(xdotool getwindowname $w 2>/dev/null) $(xdotool getwindowgeometry $w 2>/dev/null | sed -n 2,3p | tr '\n' ' ')"
done
timeout 8 xdpyinfo >/dev/null && echo X-OK
