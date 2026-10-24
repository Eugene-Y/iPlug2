#!/usr/bin/env bash
# T-009: REAPER at a UI scale, one plug-in (fresh, or from a project file), xact steps, then every shot cropped to
# the editor view (<shot>-view.png) for side-by-side with the mac references. Runs INSIDE the GUI container.
# Usage: t009-capture.sh <outdir> "<FMT: Name>" <plugin> <steps> [project.RPP]
# Env: REAPER_UI_SCALE (default 2).
set -uo pipefail
OUT="$1"; FX="$2"; P="$3"; STEPS="$4"; RPP="${5:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
export REAPER_UI_SCALE="${REAPER_UI_SCALE:-2}"
mkdir -p "$OUT"
rpr() { "$HERE/rpr.sh" "$@"; }
# A window manager's frame carries the client's title (Mutter on the VM): take REAPER's own window.
fx_win() { for c in $(xdotool search --onlyvisible --name "$1"); do xprop -id "$c" WM_CLASS | grep -q '"REAPER"' && echo "$c"; done | tail -1; }
view_geom() {
  local w; w=$(fx_win "$1")
  [ -n "$w" ] || { echo "0 0 0 0"; return; }
  xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print (r ? r : "0 0 0 0") }'
}
"$HERE/reaper-up.sh" "$P"
if [ -n "$RPP" ]; then
  rpr "reaper.Main_openProject(\"$(cd "$(dirname "$RPP")" && pwd)/$(basename "$RPP")\"); out('opened')"
  sleep 3
  rpr "local t=reaper.GetTrack(0,0); out('fx', reaper.TrackFX_GetCount(t)); reaper.TrackFX_Show(t,0,3)"
else
  rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
fi
sleep 4
# Gneiss grows from 200 to 620 points with MORPH on; near the screen's bottom the window manager would move the window
# up, and the steps' clicks would miss. 40: below the VM's GNOME top bar.
FXW=$(fx_win "${FX#*: }.*Track 1")
[ -n "$FXW" ] && { if command -v wmctrl >/dev/null; then wmctrl -i -r "$FXW" -e 0,20,40,-1,-1; else xdotool windowmove "$FXW" 20 40; fi; sleep 1; }
read -r X Y W H < <(view_geom "${FX#*: }.*Track 1")
echo "view $X $Y $W $H"
[ "$W" -gt 0 ] || { echo "no FX window: nothing to crop"; pkill -x reaper; exit 1; }
(echo "origin $X $Y"; cat "$STEPS") | OUT="$OUT" XACT_SCALE="$REAPER_UI_SCALE" "$HERE/xact.sh" | tee "$OUT/xact.log"
for f in "$OUT"/*.png; do
  case "$f" in *-view.png) continue;; esac
  convert "$f" -crop "${W}x${H}+${X}+${Y}" +repage "${f%.png}-view.png"
done
xwininfo -root -tree > "$OUT/xwininfo-tree.txt"
pkill -x reaper || true
