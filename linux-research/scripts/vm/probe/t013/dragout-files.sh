# Drag IPlugControls' IPLUG_TEST_DRAGOUT square (view +712,+702) out of the editor onto a GNOME Files window showing
# <dst> (already open); log the displayed cursor (XFixes, /tmp/cursor-probe) along the way and what lands in <dst>.
# Usage: dragout-files.sh <out> <view-x> <view-y> <dst> [escape]   (run through vm-x.sh after reaper-open.sh)
# Needs /tmp/cursor-probe: gcc -O1 -o /tmp/cursor-probe scripts/cursor-probe.c -lX11 -lXfixes
OUT="$1"; VX="$2"; VY="$3"; DST="$4"; ESC="${5:-}"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
# Files must already show <dst> (start it with GDK_DEBUG=dnd if wanted): a nautilus started here can stay unmapped
# while its first start (Tracker) runs, and the drop then lands on whatever is below.
N=$(for w in $(xdotool search --onlyvisible --name "^$(basename "$DST")\$" 2>/dev/null); do xprop -id "$w" WM_STATE | grep -q Normal && echo "$w"; done | tail -1)
[ -n "$N" ] || { log "no Files window"; exit 1; }
wmctrl -i -r "$N" -e 0,20,80,400,500; wmctrl -i -a "$N"; sleep 1
log "Files window $N XdndAware: $(xprop -id "$N" XdndAware 2>&1 | cut -c1-60)"
log "before: $(ls "$DST" | tr '\n' ' ')"
SX=$((VX + 712)); SY=$((VY + 702)); TX=200; TY=330
xdotool mousemove $SX $SY; sleep 0.4; log "cursor over the square: $(/tmp/cursor-probe)"
xdotool mousedown 1; sleep 0.3
for i in 1 2 3 4 5 6 7 8 9 10; do
  xdotool mousemove $((SX + (TX - SX) * i / 10)) $((SY + (TY - SY) * i / 10)); sleep 0.15
  log "at $((SX + (TX - SX) * i / 10)),$((SY + (TY - SY) * i / 10)): $(/tmp/cursor-probe)"
done
sleep 0.5; xdotool mousemove $((TX + 2)) $((TY + 1)); sleep 0.5
log "over Files: $(/tmp/cursor-probe)"
import -window root "$OUT/over-files.png"
[ -n "$ESC" ] && { xdotool key Escape; sleep 0.3; log "after Escape: $(/tmp/cursor-probe)"; }
xdotool mouseup 1; sleep 2
log "after release: $(/tmp/cursor-probe)"
import -window root "$OUT/after.png"
log "after: $(ls "$DST" | tr '\n' ' ')"
grep -a 'IPLUG_TEST_DRAGOUT' /tmp/reaper.log | tail -1 | tee -a "$R"
timeout 8 xdpyinfo >/dev/null && log X-OK
