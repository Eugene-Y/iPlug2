# Drag-out into REAPER: the harness REAPER with IPlugControls (IPLUG_TEST_DRAGOUT, IPLUG_TEST_DRAG_FILE set by the
# caller) in a floating FX window, its square dragged onto REAPER's track panel, then REAPER's tracks and their media.
# One script on purpose: rpr.sh run from a later vm-x.sh session started a second REAPER instead of reaching this one.
# Usage: dragout-reaper.sh <out> <plugin> "<FMT: Name>"   (needs /tmp/cursor-probe, see dragout-to.sh)
OUT="$1"; P="$2"; FX="$3"
S="$(cd "$(dirname "$0")/../../.." && pwd)"
mkdir -p "$OUT"
V=$(bash "$(dirname "$0")/reaper-open.sh" "$P" "$FX" | awk '/^fxwin/{print $4, $5}')
[ -n "$V" ] || { echo "no editor"; exit 1; }
M=$(for w in $(xdotool search --onlyvisible --name ' - REAPER v'); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
wmctrl -i -r "$M" -e 0,80,40,1000,800
"$S/rpr.sh" 'out("tracks before", reaper.CountTracks(0))' | tee "$OUT/reaper-tracks.txt"
bash "$(dirname "$0")/dragout-to.sh" "$OUT" ${V% *} ${V#* } 200 330 /tmp/reaper.log
sleep 1
"$S/rpr.sh" 'out("tracks after", reaper.CountTracks(0)); for i=0,reaper.CountTracks(0)-1 do local t=reaper.GetTrack(0,i); local _,n=reaper.GetTrackName(t); local it=reaper.GetTrackMediaItem(t,0); local f="-"; if it then f=reaper.GetMediaSourceFileName(reaper.GetMediaItemTake_Source(reaper.GetActiveTake(it)),"") end; out("track", i+1, n, f) end' | tee -a "$OUT/reaper-tracks.txt"
