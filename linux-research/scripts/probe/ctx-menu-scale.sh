#!/usr/bin/env bash
# T-007: where REAPER's VST3 parameter context menu (IContextMenu::popup) opens at a REAPER UI scale, for a held
# right click on IPlugControls' IVKnobControl. Runs INSIDE a GUI container (Docker xdotool clicks release in the same
# millisecond, which GTK takes as a click that closes the menu, hence the held button).
# Usage: ctx-menu-scale.sh <outdir> <plugin.vst3> <scale>
set -uo pipefail
OUT="$1"; PLUG="$2"; S="$3"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
REAPER_UI_SCALE="$S" "$HERE/reaper-up.sh" "$PLUG" >/dev/null
"$HERE/rpr.sh" 'reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,"VST3: IPlugControls",false,-1); reaper.TrackFX_Show(t,0,3)' >/dev/null
sleep 3
w=$(xdotool search --onlyvisible --name "IPlugControls.*Track 1" | tail -1)
read -r OX OY < <(xwininfo -id "$w" -tree | awk '/^ +0x/{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
PX=$(awk -v o="$OX" -v s="$S" 'BEGIN{printf "%d", o + 322 * s}'); PY=$(awk -v o="$OY" -v s="$S" 'BEGIN{printf "%d", o + 237 * s}')
xdotool mousemove "$PX" "$PY" mousedown 3; sleep 0.25; xdotool mouseup 3; sleep 1.5
import -window root "$OUT/menu.png"
M=$(xdotool search --onlyvisible --class "reaper" 2>/dev/null | while read -r id; do xwininfo -id "$id" | awk -v id="$id" '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} /Width:/{w=$NF} /Height:/{h=$NF} END{if (w < 600 && h > 40) print id, x, y, w, h}'; done | tail -1)
echo "scale $S: right click at screen $PX,$PY (view 322,237); popup window: ${M:-none found}" | tee "$OUT/menu.txt"
xdotool key Escape
