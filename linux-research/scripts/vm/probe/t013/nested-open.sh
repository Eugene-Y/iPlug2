#!/usr/bin/env bash
# The harness REAPER on the nested GNOME Shell's Xwayland with one plug-in in a floating FX window, then the view in
# the outer screen's coordinates. Usage: nested-open.sh <plugin> "<FMT: Name>" [x,y]
# (run through vm-x.sh; REAPER's stderr is /tmp/reaper.log)
P="$1"; FX="$2"; POS="${3:-200,60}"
D="$(cd "$(dirname "$0")" && pwd)"; S="$D/../../.."
. "$D/nested-env.sh" || exit 1
"$S/reaper-up.sh" "$P"
"$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 3
W=$(for w in $(xdotool search --onlyvisible --name "${FX#*: }.*Track 1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
[ -n "$W" ] || { echo "no FX window"; exit 1; }
wmctrl -i -r "$W" -e "0,${POS%,*},${POS#*,},-1,-1"; sleep 1
bash "$D/nested-view.sh" "${FX#*: }.*Track 1"
