#!/usr/bin/env bash
# T-020 drag-out in the GUI container: IPlugControls (IPLUG_TEST_DRAGOUT) in REAPER, drag its corner square onto
# (1) the xdnd-target probe, (2) the probe with Escape before the release, (3) REAPER's arrange view (a .wav).
# Usage: run.sh <out> <IPlugControls.clap|.vst3> "<FMT: Name>"
set -u
OUT="$1"; P="$2"; FX="$3"; HERE="$(cd "$(dirname "$0")" && pwd)"; R="$(cd "$HERE/../../../scripts" && pwd)"
mkdir -p "$OUT"
gcc "$HERE/xdnd-target.c" -lX11 -o /tmp/xdnd-target || exit 2
python3 - <<'PY'
import struct, wave
with wave.open('/tmp/drag me.wav', 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000); w.writeframes(b'\0\0' * 24000)
PY
export IPLUG_TEST_DRAG_FILE='/tmp/drag me.wav'
"$R/reaper-up.sh" "$P"
"$R/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 3
w=$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1)
read -r VX VY VW VH < <(xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }')
SX=$((VX + 712)); SY=$((VY + 702))  # the test square (IPlugControls layout at scale 1)
echo "view $VX $VY $VW $VH square $SX,$SY"
drag() { # <to-x> <to-y> <escape-before-release: 0|1>
  xdotool mousemove $SX $SY mousedown 1; sleep 0.3
  for i in 1 2 3 4 5 6 7 8; do xdotool mousemove $(( SX + ($1 - SX) * i / 8 )) $(( SY + ($2 - SY) * i / 8 )); sleep 0.15; done
  sleep 0.5; [ "$3" = 1 ] && { xdotool key Escape; sleep 0.3; }
  xdotool mouseup 1; sleep 1.5
}
# REAPER's arrange view (track 1's lane): the FX window covers it, so it moves right first
xdotool windowmove "$w" 560 140; sleep 1
read -r VX VY VW VH < <(xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }')
SX=$((VX + 712)); SY=$((VY + 702))
"$R/rpr.sh" "out('items before', reaper.CountMediaItems(0))" | tee "$OUT/reaper-items.txt"
drag 500 240 0
sleep 1
"$R/rpr.sh" "out('items after', reaper.CountMediaItems(0)); if reaper.CountMediaItems(0) > 0 then local it=reaper.GetMediaItem(0,0); local tk=reaper.GetActiveTake(it); local src=reaper.GetMediaItemTake_Source(tk); out('source', reaper.GetMediaSourceFileName(src, '')) end" | tee -a "$OUT/reaper-items.txt"
import -window root "$OUT/after-reaper-drop.png"
for case in drop escape; do
  /tmp/xdnd-target 1250 80 300 250 12 > "$OUT/target-$case.txt" 2>&1 & sleep 1
  # the window manager places it where it likes: aim at where it is
  eval "$(xdotool search --onlyvisible --name '^xdnd-target$' getwindowgeometry --shell | tail -5)"
  drag $((X + WIDTH / 2)) $((Y + HEIGHT / 2)) $([ $case = escape ] && echo 1 || echo 0)
  pkill -f /tmp/xdnd-target; sleep 0.5
  echo "== target ($case)"; cat "$OUT/target-$case.txt"
  echo "FX window open after the $case case: $(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | wc -l)"
done
grep -a 'IPLUG_TEST_DRAGOUT\|iPlug2' /tmp/reaper.log | tee "$OUT/plugin.txt"
pkill -x reaper
