#!/usr/bin/env bash
# Inside the GUI container, no debugger attached: N fresh REAPER sessions, open the editor once,
# report the mean brightness of the editor area (rendered IPlugControls ~0.45, unrendered/black <0.2).
# Usage: firstopen.sh <plugin> "<FMT: Name>" <N> <outdir>
set -uo pipefail
P="$1"; FX="$2"; N="$3"; OUT="$4"; S=/Volumes/Storage/code/IPlug2/linux-research/scripts
mkdir -p "$OUT"
ok=0
for i in $(seq "$N"); do
  "$S/reaper-up.sh" "$P" >/dev/null
  "$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
  sleep 3
  import -window root "$OUT/open-$i.png"
  m=$(convert "$OUT/open-$i.png" -crop 1024x768+113+188 -colorspace gray -format '%[fx:mean]' info:)
  r=$(awk -v m="$m" 'BEGIN{print (m>0.3)?"RENDERED":"BLANK"}')
  [ "$r" = RENDERED ] && ok=$((ok+1))
  echo "trial $i mean=$m $r"
done
echo "RESULT $ok/$N first opens rendered"
