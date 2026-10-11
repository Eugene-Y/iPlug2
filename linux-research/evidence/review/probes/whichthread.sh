#!/usr/bin/env bash
# Inside the GUI container. Fresh REAPER, add plugin (no debugger attached during instantiation),
# close+reopen editor, then count Draw / OnDisplayTimer / IPlugAPIBase::OnTimer hits per gdb thread (1 = main).
# Usage: whichthread.sh <plugin> "<FMT: Name>" <out>
set -uo pipefail
P="$1"; FX="$2"; OUT="$3"; S=/Volumes/Storage/code/IPlug2/linux-research/scripts
"$S/reaper-up.sh" "$P" >/dev/null
"$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 2
"$S/rpr.sh" "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; sleep 1
"$S/rpr.sh" "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 2
import -window root "$OUT.png"
{
  echo "--- thread names"; cat /proc/$(cat /tmp/reaper.pid)/task/*/comm | sort | uniq -c
  "$S/gdb-count.sh" "$(cat /tmp/reaper.pid)" 6 'iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)' \
     iplug::igraphics::IGraphicsLinux::OnDisplayTimer iplug::IPlugAPIBase::OnTimer
  echo "--- bt of the threads involved"
  gdb -q -p "$(cat /tmp/reaper.pid)" -batch -ex 'thread apply all bt 8' </dev/null 2>/dev/null | grep -E '^Thread|TimerThreadProc|Timer_impl|IPlugCLAP|OnDisplayTimer' | grep -B1 -E 'TimerThreadProc|Timer_impl|IPlugCLAP|OnDisplayTimer' | grep '^Thread\|ThreadProc'
} > "$OUT.txt" 2>&1
cat "$OUT.txt"
