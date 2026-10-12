#!/usr/bin/env bash
# Paste into the editor's text entry what REAPER itself put on the CLIPBOARD (its track name field). REAPER's main
# thread serves that selection, so only the editor's non-blocking Ctrl+V path can get it. Runs INSIDE the GUI container.
# Usage: host-clipboard-probe.sh <IPlugControls.clap|.vst3> <outdir>
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PLUG="$1"; OUT="$2"; FX="$(echo "${PLUG##*.}" | tr a-z A-Z): IPlugControls"
mkdir -p "$OUT"; : > "$OUT/xact-fails.txt"
"$HERE/reaper-up.sh" "$PLUG" >/dev/null
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true)" >/dev/null; sleep 1
# track 1's name field in the track panel (REAPER 7 default theme, 1600x1000 screen)
xdotool mousemove 190 219 click --repeat 2 --delay 80 1; sleep 0.7
xdotool type 77; xdotool key ctrl+a ctrl+c; sleep 0.3; xdotool key Return; sleep 0.5
echo "xclip reads [$(timeout 3 xclip -o -selection clipboard)] from REAPER" | tee "$OUT/probe.txt"
"$HERE/rpr.sh" "local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 3
W=$(xdotool search --name "IPlugControls.*Track 1" | tail -1)
read -r OX OY < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
printf 'origin %s %s\ndclick 941 437\nwait 1\nkey ctrl+v\nwait 0.5\nkey Return\nwait 0.5\nexpect-param 0 0 ^77\n' "$OX" "$OY" | OUT="$OUT" "$HERE/xact.sh" | tee -a "$OUT/probe.txt"
[ -s "$OUT/xact-fails.txt" ] && { echo "RESULT: FAIL" | tee -a "$OUT/probe.txt"; exit 1; }
echo "RESULT: PASS" | tee -a "$OUT/probe.txt"
