#!/usr/bin/env bash
# The editor opens at another size than the host asked for (get_size) before the open: IPlugEffect keeps its UI
# draw scale in its state and applies it in its graphics factory. Corner-resize instance 1, copy it (with its state) to
# a 2nd track, open the copy; log every resize call and the sizes of REAPER's container and the editor.
# Runs INSIDE the GUI container (gx.sh), aarch64 only. IPlugEffect only: the corner resizer's coordinates are its own.
# Usage: restore-size-probe.sh <outdir> <FX> <plugin> <scale>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; SCALE="$4"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
REAPER_UI_SCALE="$SCALE" "$HERE/reaper-up.sh" "$PLUG"
RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); reaper.InsertTrackAtIndex(1,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)"
sleep 3
view() { local w; w=$(xdotool search --onlyvisible --name "${FX#*: }.*Track $1" | tail -1)
  xwininfo -id "$w" -tree | awk '/^ +0x/{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }'; }
read -r OX OY W H < <(view 1)
echo "drag 446 302 560 380" | OUT="$OUT" XACT_ORIGIN="$OX $OY" XACT_SCALE="$SCALE" "$HERE/xact.sh" >/dev/null
sleep 1; read -r _ _ W H < <(view 1); echo "instance 1 after the corner resize: container ${W}x${H}" | tee "$OUT/sizes.txt"
# instance 1 stays open: IPlugEffect saves the open editor's draw scale (a closed one saves its pending scale)

brk() { printf 'break %s\ncommands\nsilent\nprintf "%s\\n"%s\ncontinue\nend\n' "$1" "$2" "${3:+, $3}"; }
{
  echo "set pagination off"; echo "set breakpoint pending on"
  brk 'iplug::IPlugAPIBase::EditorResizeFromUI' 'EditorResizeFromUI %d x %d, needsPlatformResize %d' '$w1, $w2, $w3'
  brk 'iplug::igraphics::IGEditorDelegate::OnParentWindowResize' 'host resize (OnParentWindowResize) %d x %d' '$w1, $w2'
  brk 'iplug::igraphics::IGraphicsLinux::TellHostTheOpenedSize' 'TellHostTheOpenedSize'
  echo "continue"
} > "$OUT/calls.gdb"
timeout -s INT 30 gdb -q -batch -p "$RPID" -x "$OUT/calls.gdb" </dev/null > "$OUT/calls-raw.txt" 2>&1 &
GDB=$!
sleep 4
"$HERE/rpr.sh" "local a=reaper.GetTrack(0,0); local b=reaper.GetTrack(0,1); reaper.TrackFX_CopyToTrack(a,0,b,0,false); reaper.TrackFX_Show(b,0,3)"
sleep 4
pkill -INT -f "gdb -q -batch -p $RPID" 2>/dev/null; wait $GDB 2>/dev/null
grep -E '^(EditorResizeFromUI|host resize|TellHost)' "$OUT/calls-raw.txt" | tee "$OUT/calls.txt"
read -r _ _ W H < <(view 2)
echo "copy on track 2: container ${W}x${H}" | tee -a "$OUT/sizes.txt"
xwininfo -id "$(xdotool search --onlyvisible --name "${FX#*: }.*Track 2" | tail -1)" -tree | grep -E '^ +0x' | tee -a "$OUT/sizes.txt"
import -window root "$OUT/copy-open.png"
