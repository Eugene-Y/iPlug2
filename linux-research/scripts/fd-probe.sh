#!/usr/bin/env bash
# How often the host delivers the editor's X fd callbacks, per fd, in each phase (first open, reopen, 2 instances).
# Runs INSIDE the GUI container. Usage: fd-probe.sh <outdir> "<FMT: Name>" <plugin> [secs=4]
# Counts HostLoop::Dispatch(isFd=true) hits by fd while the mouse moves over the editor (aarch64: w1 = isFd, x2 = handle).
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; SECS="${4:-4}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
wiggle() { local w; w=$(xdotool search --onlyvisible --name "$1" | tail -1); [ -n "$w" ] || return
  eval "$(xdotool getwindowgeometry --shell "$w")"
  for i in $(seq 0 15); do xdotool mousemove $((X + 40 + i * 20)) $((Y + 100 + (i % 2) * 150)); sleep 0.2; done; }
probe() { # <phase> <window-regex>
  local G; G=$(mktemp /tmp/fdp-XXXX)
  printf '%s\n' "set pagination off" "break iplug::HostLoop::Dispatch(bool, unsigned long, int) if \$w1 == 1" \
    "commands" "silent" "printf \"FD %d\\n\", \$x2" "continue" "end" "continue" > "$G"
  ( sleep 1; wiggle "$2" ) &
  timeout -s INT "$SECS" gdb -q -batch -p "$RPID" -x "$G" </dev/null 2>/dev/null | grep '^FD' | sort | uniq -c | sed "s/^/$1 /" | tee -a "$OUT/fd-probe.txt"
  wait
  echo "$1 X fds of REAPER: $(ls -l /proc/$RPID/fd 2>/dev/null | grep -c socket) sockets" | tee -a "$OUT/fd-probe.txt"
}
: > "$OUT/fd-probe.txt"
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 3
probe first-open "${FX#*: }.*Track 1"
"$HERE/rpr.sh" "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; sleep 1
"$HERE/rpr.sh" "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 2
probe reopen "${FX#*: }.*Track 1"
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(1,true); local t=reaper.GetTrack(0,1); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 3
probe two-instances "${FX#*: }.*Track 2"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null; true
