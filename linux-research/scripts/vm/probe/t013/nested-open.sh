# The harness REAPER on the nested GNOME Shell's Xwayland with one plug-in in a floating FX window, then the view in
# the outer screen's coordinates. REAPER's update notice (a modal over the evaluation nag) is closed politely first.
# Usage: nested-open.sh <plugin> "<FMT: Name>" [x,y]   (run through vm-x.sh; REAPER's stderr is /tmp/reaper.log)
P="$1"; FX="$2"; POS="${3:-200,60}"
D="$(cd "$(dirname "$0")" && pwd)"; S="$D/../../.."
. "$D/nested-env.sh"
"$S/reaper-up.sh" "$P" &
# reaper-up first stops a running REAPER, whose windows would end the waits early.
sleep 3
for i in $(seq 60); do xdotool search --onlyvisible --name 'REAPER New Version Notification' >/dev/null 2>&1 && break; sleep 0.5; done
for n in "REAPER New Version Notification" "^About REAPER"; do
  for i in $(seq 15); do
    ws=$(xdotool search --onlyvisible --name "$n" 2>/dev/null) || break
    for w in $ws; do wmctrl -i -c "$w"; done; sleep 1
  done
done
wait
"$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 3
W=$(for w in $(xdotool search --onlyvisible --name "${FX#*: }.*Track 1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
[ -n "$W" ] || { echo "no FX window"; exit 1; }
wmctrl -i -r "$W" -e "0,${POS%,*},${POS#*,},-1,-1"; sleep 1
bash "$D/nested-view.sh" "${FX#*: }.*Track 1"
