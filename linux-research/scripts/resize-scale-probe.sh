#!/usr/bin/env bash
# Plug-in initiated resize (corner resizer) at a REAPER UI scale: every resize call between the editor and REAPER for
# 6 s after the drag (a loop shows as calls that keep coming), and the final sizes of REAPER's container and the editor.
# Runs INSIDE the GUI container (gx.sh), aarch64 only (reads arguments from registers).
# Usage: resize-scale-probe.sh <outdir> <FX e.g. "CLAP: IPlugEffect"> <plugin> <scale> <drag X1 Y1 X2 Y2, logical>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; SCALE="$4"; shift 4; DRAG="$*"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
REAPER_UI_SCALE="$SCALE" "$HERE/reaper-up.sh" "$PLUG"
RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); local i=reaper.TrackFX_AddByName(t,\"$FX\",false,-1); out('add',i); reaper.TrackFX_Show(t,0,3)"
sleep 3
view() { local w; w=$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1)
  xwininfo -id "$w" -tree | awk '/^ +0x/{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }'; }
read -r OX OY W H < <(view); echo "before: container ${W}x${H}" | tee "$OUT/sizes.txt"
import -window root "$OUT/before.png"

brk() { printf 'break %s\ncommands\nsilent\nprintf "%s\\n"%s\ncontinue\nend\n' "$1" "$2" "${3:+, $3}"; }
{
  echo "set pagination off"; echo "set breakpoint pending on"
  brk 'iplug::IPlugAPIBase::EditorResizeFromUI' 'EditorResizeFromUI %d x %d, needsPlatformResize %d' '$w1, $w2, $w3'
  brk 'iplug::igraphics::IGEditorDelegate::OnParentWindowResize' 'host resize (OnParentWindowResize) %d x %d' '$w1, $w2'
  brk 'iplug::igraphics::IGraphics::Resize' 'IGraphics::Resize %d x %d, draw scale %g' '$w1, $w2, $s0'
  echo "continue"
} > "$OUT/calls.gdb"
timeout -s INT 40 gdb -q -batch -p "$RPID" -x "$OUT/calls.gdb" </dev/null > "$OUT/calls-raw.txt" 2>&1 &
GDB=$!
sleep 4
echo "drag $DRAG" | OUT="$OUT" XACT_ORIGIN="$OX $OY" XACT_SCALE="$SCALE" "$HERE/xact.sh"
sleep 6
pkill -INT -f "gdb -q -batch -p $RPID" 2>/dev/null; wait $GDB 2>/dev/null
grep -E '^(EditorResizeFromUI|host resize|IGraphics::Resize)' "$OUT/calls-raw.txt" > "$OUT/calls.txt"
read -r OX OY W H < <(view)
ED=$(xwininfo -root -tree | awk -v w="$W" -v h="$H" '$0 ~ " "w"x"h"\\+0\\+0 " {print $1; exit}')
echo "after: container ${W}x${H}; editor window with the container's size: ${ED:-none}" | tee -a "$OUT/sizes.txt"
xwininfo -id "$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1)" -tree | grep -E '^ +0x' >> "$OUT/sizes.txt"
import -window root "$OUT/after.png"
echo "calls after the drag: $(wc -l < "$OUT/calls.txt")"; cat "$OUT/calls.txt" | uniq -c | tail -15
