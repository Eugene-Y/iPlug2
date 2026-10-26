#!/usr/bin/env bash
# Multi-touch on IPlugControls in REAPER on the VM: two fingers on two controls at once, one finger on a knob, then
# the mouse. Needs vtouch.py running on FIFO (default /tmp/t030-touch.fifo) before the editor opens.
# Usage (through vm-run.sh): vm/probe/touch-reaper.sh <out> "<FMT: IPlugControls>" <plugin>
# Writes params-*.txt (gain, Freq 1, Freq 2 as REAPER formats them) and shots; prints PASS/FAIL lines.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; FIFO="${FIFO:-/tmp/t030-touch.fifo}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
[ -p "$FIFO" ] || { echo "FAIL: no touch FIFO $FIFO"; exit 1; }
"$HERE/reaper-up.sh" "$PLUG"
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 3
w=$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1)
read -r OX OY < <(xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
echo "view origin $OX $OY"
params() { "$HERE/rpr.sh" "local t=reaper.GetTrack(0,0); for _,p in ipairs({0,2,3}) do local _,v=reaper.TrackFX_GetFormattedParamValue(t,0,p,''); out(p, v) end" | tee "$OUT/params-$1.txt"; }
fingers() { echo "$*" > "$FIFO"; }
at() { echo "$1 $(( OX + $2 )) $(( OY + $3 ))"; } # slot, view x, view y -> slot, screen x, screen y

params before
# IVKnobControl (gain) dragged down and the XY pad (Freq 1/2) dragged up-right, both at once
fingers "drag 800 16 $(at 0 322 232) $(at 0 322 332 | cut -d' ' -f2-) $(at 1 144 446) $(at 1 218 359 | cut -d' ' -f2-)"
sleep 1.3; import -window root "$OUT/t1-two-fingers-held.png"
fingers "up 0; up 1; syn"; sleep 0.8
params two-fingers
# One finger on ISVGKnobControl (gain)
fingers "drag 500 10 $(at 0 68 232) $(at 0 68 292 | cut -d' ' -f2-); sleep 0.2; up 0; syn"; sleep 1.2
params one-finger
import -window root "$OUT/t2-one-finger.png"
# The mouse: the first press after a touch is lost on this desktop (xev too), so one click on the background first
xdotool mousemove $(( OX + 948 )) $(( OY + 599 )) mousedown 1 sleep 0.08 mouseup 1; sleep 0.5
xdotool mousemove $(( OX + 948 )) $(( OY + 599 )) mousedown 1 sleep 0.08 mouseup 1; sleep 0.5
xdotool mousemove $(( OX + 322 )) $(( OY + 232 )) mousedown 1; sleep 0.3
for dy in 10 20 30 40; do xdotool mousemove $(( OX + 322 )) $(( OY + 232 + dy )); sleep 0.15; done
sleep 0.3; xdotool mouseup 1; sleep 0.8
params mouse
import -window root "$OUT/t3-mouse.png"

g() { sed -n "s/^$2 //p" "$OUT/params-$1.txt"; }
check() { [ "$(g "$1" "$3")" != "$(g "$2" "$3")" ] && echo "PASS: param $3 changed $1 -> $2" || echo "FAIL: param $3 unchanged $1 -> $2"; }
check before two-fingers 0; check before two-fingers 2; check before two-fingers 3
check two-fingers one-finger 0
check one-finger mouse 0

