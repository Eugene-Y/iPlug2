#!/usr/bin/env bash
# Which threads run the P3b editor paths (keys, focus, clipboard, cursors, stand-in menus and text entry) while a
# steps script drives the editor. Runs INSIDE the GUI container. gdb slows REAPER down, so step checks may fail
# here; only the thread ids count (gdb thread 1 = REAPER's main thread).
# Usage: input-thread-check.sh <plugin> <FX name e.g. "CLAP: IPlugControls"> <steps> <outdir>
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
PLUG="$1"; FX="$2"; STEPS="$3"; OUT="$4"
mkdir -p "$OUT"
"$HERE/reaper-up.sh" "$PLUG" >/dev/null
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 3
W=$(xdotool search --name "${FX#*: }.*Track 1" | tail -1)
read -r OX OY < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
P=$(cat /tmp/reaper.pid)
L=iplug::igraphics::IGraphicsLinux
"$HERE/gdb-count.sh" "$P" 150 "$L::HandleKey" "$L::TakeKeyboardFocus" "$L::CheckKeyboardFocus" "$L::ReleaseKeyboardFocus" \
  "$L::FitMenusOnScreen" "$L::FinishFadingMenu" "$L::UpdateTopLevelHidden" \
  "$L::SetTextInClipboard" "$L::GetTextFromClipboard" "$L::CreatePlatformTextEntry" "$L::CreatePlatformPopupMenu" \
  "$L::SetMouseCursor" "$L::HideMouseCursor" "$L::MoveMouseCursor" "$L::ProcessXEvents()" > "$OUT/input-threads.txt" 2>&1 &
G=$!
sleep 8
(echo "origin $OX $OY"; sed "s/__PROC__/$P/" "$STEPS") | OUT="$OUT" "$HERE/xact.sh" > "$OUT/xact.log" 2>&1
pkill -INT -f "gdb -q -batch -p $P"; wait $G
cat "$OUT/input-threads.txt"
if grep HIT "$OUT/input-threads.txt" | grep -qv 'tid=1$'; then echo "OFF-MAIN"; exit 1; fi
grep -q HIT "$OUT/input-threads.txt" || { echo "NO HITS"; exit 1; }
echo "all on the main thread"
