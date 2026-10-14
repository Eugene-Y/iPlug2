#!/usr/bin/env bash
# REAPER with one instance of the given plug-in on track 1, editor open, IPLUG_TEST_DIALOG_DIR set to the dialog
# fixture. Runs INSIDE the GUI container. Prints "origin X Y" of the editor view.
# Usage: dialog-up.sh "<FMT: Name>" <plugin>
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
FX="$1"; PLUG="$2"
export IPLUG_TEST_DIALOG_DIR="$("$HERE/dialog-fixture.sh")"
"$HERE/reaper-up.sh" "$PLUG" >/dev/null
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
w=$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1)
xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print "origin " r }'
