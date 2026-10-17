#!/usr/bin/env bash
# REAPER + two instances of a CLAP, mouse sweeps over the 2nd editor (as reaper-plugin-test's count does),
# optionally while gdb-count.sh counts (COUNT_SECS > 0). Runs INSIDE a GUI container.
# Usage: [COUNT_SECS=20] hover-stress.sh <plugin.clap> <rounds>
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
P="$1"; N="$2"; NAME=$(basename "$P" .clap)
REAPER_EXIT_FILE=/tmp/reaper.exit "$HERE/reaper-up.sh" "$P" >/dev/null
RPID=$(cat /tmp/reaper.pid)
for t in 0 1; do
  "$HERE/rpr.sh" "reaper.InsertTrackAtIndex($t,true); local tr=reaper.GetTrack(0,$t); reaper.TrackFX_AddByName(tr,\"CLAP: $NAME\",false,-1); reaper.TrackFX_Show(tr,0,3)" >/dev/null
  sleep 3
done
w=$(xdotool search --onlyvisible --name "$NAME.*Track 2" | tail -1)
read -r X Y W H < <(xwininfo -id "$w" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} /Width/{w=$NF} /Height/{h=$NF} END{print x,y,w,h}')
echo "editor $X $Y $W $H"
for r in $(seq "$N"); do
  if [ "${COUNT_SECS:-0}" -gt 0 ]; then
    ( sleep $((COUNT_SECS - 4)); for i in $(seq 0 19); do xdotool mousemove $((X + W * i / 20)) $((Y + H * (i % 3 + 1) / 4)); sleep 0.2; done ) &
    hits=$("$HERE/gdb-count.sh" "$RPID" "$COUNT_SECS" 'iplug::igraphics::IGraphicsLinux::OnDisplayTimer' 'iplug::igraphics::IGraphicsLinux::ProcessXEvents()' 'iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)' | awk '{s+=$1} END{print s+0}')
    wait
    echo "round $r: $hits hits"
  else
    for i in $(seq 0 19); do xdotool mousemove $((X + W * i / 20)) $((Y + H * (i % 3 + 1) / 4)); sleep 0.2; done
  fi
  sleep 1
  if kill -0 "$RPID" 2>/dev/null; then echo "round $r: alive"; else sleep 1; echo "round $r: REAPER DEAD, exit status $(cat /tmp/reaper.exit 2>/dev/null)"; tail -5 /tmp/reaper.log; exit 1; fi
done
echo "HOVER PASS ($N rounds)"
pkill -x reaper
