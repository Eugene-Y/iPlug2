#!/usr/bin/env bash
# Scratch helper (inside the GUI container): fresh REAPER with one plug-in, editor open, then xact steps.
# Usage: iter.sh <plugin> <FX name> <steps> <outdir>
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
"$HERE/reaper-up.sh" "$1" >/dev/null
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); local i=reaper.TrackFX_AddByName(t,\"$2\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
W=$(xdotool search --name "${2#*: }.*Track 1" | tail -1)
read -r OX OY < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
mkdir -p "$4"
(echo "origin $OX $OY"; sed "s/__PROC__/$(cat /tmp/reaper.pid)/" "$3") | OUT="$4" "$HERE/xact.sh"
