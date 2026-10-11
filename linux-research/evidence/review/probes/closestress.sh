#!/usr/bin/env bash
# Inside a GUI container. Open/close the editor ROUNDS x 10 times (10 toggles per ReaScript call, so the
# host thread closes while the plugin's render/timer threads are mid-tick). Detects a hung host main thread.
# Usage: closestress.sh <plugin> "<FMT: Name>" <rounds> <out>
set -uo pipefail
P="$1"; FX="$2"; N="$3"; OUT="$4"; S=/Volumes/Storage/code/IPlug2/linux-research/scripts
"$S/reaper-up.sh" "$P" >/dev/null
PID=$(cat /tmp/reaper.pid)
"$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 2
for i in $(seq "$N"); do
  r=$(timeout 15 "$S/rpr.sh" "local t=reaper.GetTrack(0,0); for k=1,10 do reaper.TrackFX_Show(t,0,2); reaper.TrackFX_Show(t,0,3); local e=os.clock()+math.random()*0.05; while os.clock()<e do end end; out('ok')")
  st=$(ps -o stat= -p "$PID" 2>/dev/null)
  if [ -z "$st" ] || [[ "$st" == Z* ]]; then echo "round $i: REAPER DIED"; exit 1; fi
  if [ "$r" != ok ]; then
    echo "round $i: NO ANSWER (host main thread hung?) -> $r"
    gdb -q -p "$PID" -batch -ex 'thread apply all bt 12' </dev/null > "$OUT.hang-bt.txt" 2>&1
    import -window root "$OUT.hang.png"; exit 2
  fi
  sleep 0.$((RANDOM % 5))
done
import -window root "$OUT.png"
echo "OK: $((N*10)) close/open cycles, REAPER responsive"
