#!/usr/bin/env bash
# Inside a GUI container: instantiate N instances (no debugger attached), then count live IPlugTimer threads.
# Usage: timercount.sh <plugin> "<FMT: Name>" <N>
set -uo pipefail
P="$1"; FX="$2"; N="$3"; S=/Volumes/Storage/code/IPlug2/linux-research/scripts
"$S/reaper-up.sh" "$P" >/dev/null
"$S/rpr.sh" "for i=0,$N-1 do reaper.InsertTrackAtIndex(i,true); reaper.TrackFX_AddByName(reaper.GetTrack(0,i),\"$FX\",false,-1) end; out('added')"
sleep 2
n=$(gdb -q -p "$(cat /tmp/reaper.pid)" -batch -ex 'thread apply all bt 8' </dev/null 2>/dev/null | grep -c 'Timer_impl::ThreadProc')
echo "$FX: $n live IPlugTimer threads for $N instances"
