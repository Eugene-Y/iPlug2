#!/usr/bin/env bash
# What REAPER tells an editor about scale and size, and the sizes of the windows that result.
# Runs INSIDE the GUI container (gx.sh), aarch64 only (reads arguments from registers).
# Usage: scale-probe.sh <outdir> <FX e.g. "CLAP: IPlugEffect"> <plugin> [Xft.dpi, - for none, keep (default)]
# Writes calls.txt (host calls with their arguments, editor size updates), tree.txt (xwininfo), open.png.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; DPI="${4:-keep}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
if [ "$DPI" = keep ]; then xprop -root RESOURCE_MANAGER; else "$HERE/xft-dpi.sh" "$DPI"; fi > "$OUT/resources.txt" 2>&1
"$HERE/reaper-up.sh" "$PLUG"
RPID=$(cat /tmp/reaper.pid)

CMDS="$OUT/calls.gdb"
brk() { # <symbol> <printf format> <printf args>
  printf 'break %s\ncommands\nsilent\nprintf "%s\\n"%s\ncontinue\nend\n' "$1" "$2" "${3:+, $3}"
}
{
  echo "set pagination off"; echo "set breakpoint pending on"
  brk "'iplug::IPlugCLAP::guiSetScale(double)'" 'clap set_scale %g' '$d0'
  brk "'iplug::IPlugCLAP::guiSetSize(unsigned int, unsigned int)'" 'clap set_size %d x %d' '$w1, $w2'
  brk "'iplug::IPlugCLAP::guiGetSize(unsigned int*, unsigned int*)'" 'clap get_size'
  brk "'iplug::IPlugCLAP::EditorResize(int, int)'" 'clap EditorResize (request_resize if changed) %d x %d' '$w1, $w2'
  brk "'IPlugVST3View<iplug::IPlugVST3>::setContentScaleFactor(float)'" 'vst3 setContentScaleFactor %g' '$s0'
  brk "'IPlugVST3View<iplug::IPlugVST3>::getSize(Steinberg::ViewRect*)'" 'vst3 getSize'
  brk "'IPlugVST3View<iplug::IPlugVST3>::onSize(Steinberg::ViewRect*)'" 'vst3 onSize %d x %d' '*(int*)($x1+8) - *(int*)$x1, *(int*)($x1+12) - *(int*)($x1+4)'
  brk "'iplug::IPlugVST3::EditorResize(int, int)'" 'vst3 EditorResize (resizeView if changed) %d x %d' '$w1, $w2'
  brk 'iplug::igraphics::IGEditorDelegate::UpdateEditorSizeForScreenScale' 'UpdateEditorSizeForScreenScale'
  brk 'iplug::igraphics::IGEditorDelegate::SetScreenScale' 'host scale (IGEditorDelegate::SetScreenScale) %g' '$s0'
  brk 'iplug::igraphics::IGEditorDelegate::OnParentWindowResize' 'host resize (OnParentWindowResize) %d x %d' '$w1, $w2'
  brk 'iplug::IPlugAPIBase::EditorResizeFromUI' 'EditorResizeFromUI %d x %d, needsPlatformResize %d' '$w1, $w2, $w3'
  brk 'iplug::igraphics::linux_scale::QueryDesktopScale' 'QueryDesktopScale'
  brk 'iplug::igraphics::IGraphics::SetScreenScale' 'IGraphics::SetScreenScale %g' '$s0'
  echo "continue"
} > "$CMDS"
timeout -s INT 30 gdb -q -batch -p "$RPID" -x "$CMDS" </dev/null 2>&1 > "$OUT/calls-raw.txt" 2>&1 &
GDB=$!
sleep 4
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); local i=reaper.TrackFX_AddByName(t,\"$FX\",false,-1); out('add',i); reaper.TrackFX_Show(t,0,3)"
sleep 5
import -window root "$OUT/open.png"
xwininfo -root -tree > "$OUT/tree.txt"
# on the VM gdb runs as root through sudo (scripts/vm/remote-bin/gdb): signal the sudo, which forwards it
pkill -INT -f "gdb -q -batch -p $RPID" 2>/dev/null; pkill -INT -f "^sudo -n /usr/bin/gdb -q -batch -p $RPID" 2>/dev/null; wait $GDB 2>/dev/null
grep -vE '^\[|^Breakpoint|^Reading|^Download|^$|^Using|^warning|^Function .* not defined|clock_nanosleep|^Thread .* SIGINT' "$OUT/calls-raw.txt" > "$OUT/calls.txt"
W=$(xdotool search --onlyvisible --name "${FX#*: }.*Track 1" | tail -1)
[ -n "$W" ] && xwininfo -id "$W" -tree > "$OUT/fx-window.txt"
echo "--- $FX, Xft.dpi $DPI"; cat "$OUT/calls.txt"; grep -E '^ +0x' "$OUT/fx-window.txt" 2>/dev/null | head -8
