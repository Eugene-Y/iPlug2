#!/usr/bin/env bash
# Inside the GUI container: fresh REAPER, gdb trace of timer/window lifecycle, add plugin on 2 tracks.
# Usage: probe.sh <plugin> "<FMT: Name>" <out>
set -uo pipefail
P="$1"; FX="$2"; OUT="$3"; S=/Volumes/Storage/code/IPlug2/linux-research/scripts
mkdir -p "$(dirname "$OUT")"
"$S/reaper-up.sh" "$P" >/dev/null
PID=$(cat /tmp/reaper.pid)
CMDS=$(mktemp /tmp/probe-XXXX)
{
  echo "set pagination off"; echo "set breakpoint pending on"
  echo "handle SIGPIPE nostop noprint"; echo "handle SIG32 nostop noprint"; echo "handle SIG33 nostop noprint"
  for s in 'iplug::Timer_impl::Timer_impl' 'iplug::Timer_impl::ThreadProc' 'iplug::Timer_impl::Stop' \
           'iplug::igraphics::IGraphicsLinux::StartTimer' 'iplug::igraphics::IGraphicsLinux::TimerThreadProc' \
           'iplug::igraphics::IGraphicsLinux::StopTimer' 'iplug::igraphics::IGraphicsLinux::OpenWindow' \
           'iplug::igraphics::IGraphicsLinux::CloseWindow' 'iplug::igraphics::IGEditorDelegate::CloseWindow' \
           'iplug::igraphics::IGEditorDelegate::OpenWindow'; do
    echo "break $s"; echo "commands"; echo "silent"; echo "printf \"HIT $s tid=%d\\n\", \$_thread"; echo "continue"; echo "end"
  done
  echo "continue"
} > "$CMDS"
( timeout -s INT 40 gdb -q -batch -p "$PID" -x "$CMDS" </dev/null > "$OUT.gdb" 2>&1 & )
sleep 6
"$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)"
sleep 6
"$S/rpr.sh" "reaper.InsertTrackAtIndex(1,true); local t=reaper.GetTrack(0,1); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)"
sleep 6
import -window root "$OUT.png"
sleep 25
grep -E 'HIT|exited' "$OUT.gdb" > "$OUT.trace"
cat "$OUT.trace"
