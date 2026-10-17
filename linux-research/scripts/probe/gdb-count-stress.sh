#!/usr/bin/env bash
# Stress gdb-count.sh: N attach/count/detach cycles on one REAPER with a plug-in editor open; REAPER must survive.
# Runs INSIDE a GUI container. Usage: gdb-count-stress.sh <plugin.clap> <N> [secs=5]
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
P="$1"; N="$2"; SECS="${3:-5}"; NAME=$(basename "$P" .clap)
"$HERE/reaper-up.sh" "$P" >/dev/null
RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"CLAP: $NAME\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
for i in $(seq "$N"); do
  hits=$("${GDB_COUNT:-$HERE/gdb-count.sh}" "$RPID" "$SECS" 'iplug::igraphics::IGraphicsLinux::OnDisplayTimer' 'iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)' | awk '{s+=$1} END{print s+0}')
  sleep 1
  if kill -0 "$RPID" 2>/dev/null; then echo "cycle $i: $hits hits, REAPER alive"; else echo "cycle $i: $hits hits, REAPER DEAD"; exit 1; fi
done
echo "STRESS PASS ($N cycles)"
pkill -x reaper
