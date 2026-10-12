#!/usr/bin/env bash
# Scratch: like iter.sh, with gdb printing key/focus events of the editor during the steps.
set -uo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
"$HERE/reaper-up.sh" "$1" >/dev/null
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); local i=reaper.TrackFX_AddByName(t,\"$2\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
W=$(xdotool search --name "${2#*: }.*Track 1" | tail -1)
read -r OX OY < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
mkdir -p "$4"
P=$(cat /tmp/reaper.pid)
gdb -q -batch -p $P -x "$HERE/probe/keys.gdb" </dev/null > "$4/gdb.txt" 2>&1 &
G=$!
sleep 4
(echo "origin $OX $OY"; sed "s/__PROC__/$P/" "$3") | OUT="$4" "$HERE/xact.sh" > "$4/xact.log"
kill -INT $G; sleep 1
grep -E "^(KEY|FORWARD|TAKE|RELEASE|FOCUSOUT|DISMISS|COMMIT)" "$4/gdb.txt" > "$4/events.txt"
