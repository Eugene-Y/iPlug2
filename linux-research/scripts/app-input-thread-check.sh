#!/usr/bin/env bash
# APP counterpart of input-thread-check.sh: the standalone's threads for the P3b editor paths during a steps script.
# Runs INSIDE the GUI container. Usage: app-input-thread-check.sh <app binary> <steps> <outdir>
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$1"; STEPS="$2"; OUT="$3"; NAME=$(basename "$APP")
mkdir -p "$OUT"
pkill -x "$NAME" 2>/dev/null; sleep 0.5
"$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
sleep 5
P=$(pgrep -n -x "$NAME")
ORIGIN=$(xwininfo -root -tree | awk "/\"$NAME\": \(\"$NAME\" \"$NAME\"\)/{f=1;next} f&&/^ +0x/{split(\$NF,a,\"+\"); print a[2],a[3]; exit}")
L=iplug::igraphics::IGraphicsLinux
"$HERE/gdb-count.sh" "$P" 120 "$L::HandleKey" "$L::TakeKeyboardFocus" "$L::ReleaseKeyboardFocus" \
  "$L::SetTextInClipboard" "$L::GetTextFromClipboard" "$L::CreatePlatformTextEntry" "$L::CreatePlatformPopupMenu" \
  "$L::SetMouseCursor" "$L::HideMouseCursor" "$L::MoveMouseCursor" "$L::ProcessXEvents()" > "$OUT/input-threads.txt" 2>&1 &
G=$!
sleep 6
(echo "origin $ORIGIN"; sed "s/__PROC__/$NAME/" "$STEPS") | OUT="$OUT" "$HERE/xact.sh" > "$OUT/xact.log" 2>&1
pkill -INT -f "gdb -q -batch -p $P"; wait $G
pkill -x "$NAME"
cat "$OUT/input-threads.txt"
if grep HIT "$OUT/input-threads.txt" | grep -qv 'tid=1$'; then echo "OFF-MAIN"; exit 1; fi
grep -q HIT "$OUT/input-threads.txt" || { echo "NO HITS"; exit 1; }
echo "all on the main thread"
