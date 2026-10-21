# Start the harness REAPER with one plug-in, open its editor in a floating FX window at x,y, print the view origin.
# Usage: reaper-open.sh <plugin> "<FMT: Name>" [x,y]   (run through vm-x.sh; REAPER's stderr is /tmp/reaper.log)
P="$1"; FX="$2"; POS="${3:-560,40}"
S="$(cd "$(dirname "$0")/../../.." && pwd)"   # linux-research/scripts
"$S/reaper-up.sh" "$P"
"$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 3
W=$(for w in $(xdotool search --onlyvisible --name "${FX#*: }.*Track 1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
[ -n "$W" ] || { echo "no FX window"; exit 1; }   # xwininfo -id "" would wait for a click on the screen
wmctrl -i -r "$W" -e "0,${POS%,*},${POS#*,},-1,-1"; sleep 1
read -r VX VY VW VH < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }')
echo "fxwin $W view $VX $VY $VW $VH"
timeout 8 xdpyinfo >/dev/null && echo X-OK
