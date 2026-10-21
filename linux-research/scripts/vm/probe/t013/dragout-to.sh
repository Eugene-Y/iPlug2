# Drag IPlugControls' IPLUG_TEST_DRAGOUT square (view +712,+702) out of the editor to root point x,y and release;
# log the displayed cursor (XFixes, /tmp/cursor-probe) on the way and the window chain under the target.
# Usage: dragout-to.sh <out> <view-x> <view-y> <x> <y> <host-stderr>   (run through vm-x.sh)
# Needs /tmp/cursor-probe: gcc -O1 -o /tmp/cursor-probe scripts/cursor-probe.c -lX11 -lXfixes
OUT="$1"; VX="$2"; VY="$3"; TX="$4"; TY="$5"; LOG="$6"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
log "== window chain under $TX,$TY"; bash "$HERE/drop-chain.sh" "$TX" "$TY" | tee -a "$R"
SX=$((VX + 712)); SY=$((VY + 702))
xdotool mousemove $SX $SY; sleep 0.4; xdotool mousedown 1; sleep 0.3
for i in 1 2 3 4 5 6 7 8; do
  xdotool mousemove $((SX + (TX - SX) * i / 8)) $((SY + (TY - SY) * i / 8)); sleep 0.15
  log "at $((SX + (TX - SX) * i / 8)),$((SY + (TY - SY) * i / 8)): $(/tmp/cursor-probe)"
done
sleep 0.5; xdotool mousemove $((TX + 2)) $((TY + 1)); sleep 0.5; log "over target: $(/tmp/cursor-probe)"
import -window root "$OUT/over-target.png"
xdotool mouseup 1; sleep 2
import -window root "$OUT/after.png"
grep -a 'IPLUG_TEST_DRAGOUT' "$LOG" | tail -1 | tee -a "$R"
timeout 8 xdpyinfo >/dev/null && log X-OK
