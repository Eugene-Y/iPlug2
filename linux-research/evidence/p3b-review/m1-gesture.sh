#!/usr/bin/env bash
# M1 of the P3b review, inside the GUI container: the FX window closes while the editor's menu is open.
# Opens the ICaptionControl enum menu (a parameter control: Begin on its mouse down). Counts the plug-in's Begin/EndInformHostOfParamChangeFromUI with gdb; they must pair.
# Usage: m1-gesture.sh <plugin> <outfile> [standin=0]
set -u
PLUG="$1"; OUTF="$2"; STANDIN="${3:-0}"
HERE="$(cd "$(dirname "$0")/../../scripts" && pwd)"
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
NAME=$(basename "$PLUG"); FMT=$(echo "${NAME##*.}" | tr a-z A-Z); NAME="${NAME%.*}"
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); reaper.TrackFX_AddByName(reaper.GetTrack(0,0),\"$FMT: $NAME\",false,-1); reaper.TrackFX_Show(reaper.GetTrack(0,0),0,3)" >/dev/null
sleep 3
w=$(xdotool search --onlyvisible --name "$NAME.*Track 1" | tail -1)
read -r vx vy < <(xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
if [ "$STANDIN" = 1 ]; then
  xdotool mousemove $((vx+224)) $((vy+720)) click 1; sleep 0.5
fi
( "$HERE/gdb-count.sh" "$RPID" 90 iplug::IPlugCLAP::BeginInformHostOfParamChange iplug::IPlugCLAP::EndInformHostOfParamChange > /tmp/m1-count.txt 2>&1 ) &
for i in $(seq 120); do [ "$(awk '/TracerPid/{print $2}' /proc/$RPID/status)" != 0 ] && break; sleep 0.5; done; sleep 8
xdotool mousemove $((vx+321)) $((vy+99)) click 1; sleep 1.5
import -window root "${OUTF%.txt}-menu-open.png"
"$HERE/rpr.sh" "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,2)" >/dev/null
wait
{ echo "plugin $PLUG standin=$STANDIN"; cat /tmp/m1-count.txt; ps -p "$RPID" >/dev/null && echo "REAPER alive" || echo "REAPER DEAD"; } | tee "$OUTF"
pkill -f '^/opt/REAPER/reaper' 2>/dev/null
