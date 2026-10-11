#!/usr/bin/env bash
# Resize the editor's own X window from outside (as an XEmbed-style host would), then check the editor followed.
# Runs INSIDE the GUI container. Usage: external-resize-probe.sh <outdir> "<FMT: Name>" <plugin> <W> <H>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; W="$4"; H="$5"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
"$HERE/reaper-up.sh" "$PLUG"
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 3
top=$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1)
# the editor = the deepest child of the FX window's largest child (REAPER's embed container)
ed=$(xwininfo -id "$top" -tree | awk '/^ +0x/{ split($(NF-1),g,"[x+]"); a=g[1]*g[2]; if (a >= best) { best=a; id=$1 } } END { print id }')
xwininfo -id "$ed" | grep -E "Width|Height" | tee "$OUT/before.txt"
import -window root "$OUT/before.png"
xdotool windowsize "$ed" "$W" "$H"; sleep 1.5
xwininfo -id "$ed" | grep -E "Width|Height" | tee "$OUT/after.txt"
import -window root "$OUT/after.png"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null; true
