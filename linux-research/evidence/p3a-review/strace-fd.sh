#!/usr/bin/env bash
# Which fds REAPER's main thread polls while an editor is open: is the editor's X connection in the set?
# Runs INSIDE a GUI container. Usage: strace-fd.sh <outdir> "<FMT: Name>" <plugin> [secs=3]
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; SECS="${4:-3}"
S="$(cd "$(dirname "$0")/../../scripts" && pwd)"
mkdir -p "$OUT"
"$S/reaper-up.sh" "$PLUG" >/dev/null; RPID=$(cat /tmp/reaper.pid)
socks() { ls -l /proc/$RPID/fd | awk '/socket:/{print $9}' | sort -n; }
socks > /tmp/socks-before
"$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 3
socks > /tmp/socks-after
NEW=$(comm -13 /tmp/socks-before /tmp/socks-after | tr '\n' ' ')
echo "sockets before: $(tr '\n' ' ' < /tmp/socks-before) after: $(tr '\n' ' ' < /tmp/socks-after) new: $NEW" | tee "$OUT/strace-summary.txt"
w=$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1); eval "$(xdotool getwindowgeometry --shell "$w")"
( sleep 0.5; for i in $(seq 0 15); do xdotool mousemove $((X + 40 + i * 20)) $((Y + 100 + (i % 2) * 150)); sleep 0.1; done ) &
timeout -s INT "$SECS" strace -p "$RPID" -tt -e trace=poll,ppoll,select,pselect6,epoll_wait,epoll_pwait,epoll_ctl -o "$OUT/strace.txt" 2>/dev/null
wait
total=$(grep -cE 'poll|select|epoll' "$OUT/strace.txt")
echo "poll-like calls on the main thread in ${SECS}s: $total" | tee -a "$OUT/strace-summary.txt"
for fd in $NEW; do
  echo "calls whose fd set includes fd $fd: $(grep -cE "fd=$fd[,}]|\\b$fd\\b.*in \[|\[$fd[] ,]|, $fd[],]" "$OUT/strace.txt")" | tee -a "$OUT/strace-summary.txt"
done
echo "--- sample:" >> "$OUT/strace-summary.txt"; head -5 "$OUT/strace.txt" >> "$OUT/strace-summary.txt"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null; true
