#!/usr/bin/env bash
# T-009 on the VM at REAPER scale 2, where the editor is wider than the screen: open one plug-in (fresh, or from a
# project), then shoot the FX window at two horizontal positions so that the two tiles cover the whole view.
# Usage: x2-tiles.sh <out> "<FMT: Name>" <plugin> [project.RPP]   (through vm-run.sh)
# Writes tile-<n>.png (the visible part of the view), tiles.txt ("<n> <view x of the tile's left edge> <width>"),
# geometry.txt (the FX window and view as the window manager left them).
set -uo pipefail
OUT="$1"; FX="$2"; P="$3"; RPP="${4:-}"
S="$(cd "$(dirname "$0")/../../.." && pwd)"
mkdir -p "$OUT"
REAPER_UI_SCALE=2 "$S/reaper-up.sh" "$P"
if [ -n "$RPP" ]; then
  "$S/rpr.sh" "reaper.Main_openProject(\"$(cd "$(dirname "$RPP")" && pwd)/$(basename "$RPP")\"); out('opened')"; sleep 3
  "$S/rpr.sh" "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3); out('shown')"
else
  "$S/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
fi
sleep 4
# Mutter's frame carries the client's title: take REAPER's own window. xwininfo -id "" would wait for a click.
W=$(for c in $(xdotool search --onlyvisible --name "${FX#*: }.*Track 1"); do xprop -id "$c" WM_CLASS | grep -q '"REAPER"' && echo "$c"; done | tail -1)
[ -n "$W" ] || { echo "no FX window"; exit 1; }
view() { xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }'; }
read -r SW SH < <(xdpyinfo | awk '/dimensions:/{ split($2,d,"x"); print d[1], d[2] }')
: > "$OUT/tiles.txt"
n=0
for x in 70 -10000; do
  n=$((n+1))
  read -r _ _ VW _ < <(view)
  # The second position puts the view's right edge 10 px inside the screen.
  [ "$x" = -10000 ] && x=$(( 70 - (VW - (SW - 80)) ))
  wmctrl -i -r "$W" -e "0,$x,40,-1,-1"; sleep 2
  read -r VX VY VW VH < <(view)
  { echo "position $n: asked x=$x"; xwininfo -id "$W" | grep -E "Absolute|Width|Height"; echo "view $VX $VY $VW $VH"; } >> "$OUT/geometry.txt"
  L=$(( VX < 0 ? 0 : VX )); R=$(( VX + VW > SW ? SW : VX + VW ))
  import -window root "$OUT/screen-$n.png"
  convert "$OUT/screen-$n.png" -crop "$((R - L))x${VH}+${L}+${VY}" +repage "$OUT/tile-$n.png"
  echo "$n $((L - VX)) $((R - L))" >> "$OUT/tiles.txt"
done
cat "$OUT/geometry.txt" "$OUT/tiles.txt"
