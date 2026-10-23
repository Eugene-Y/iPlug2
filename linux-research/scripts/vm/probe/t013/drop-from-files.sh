# Case 8: drag the first file of a GNOME Files window (already open, showing <dir>) to root point x,y over a host's
# plug-in editor; record the window chain under that point, Files' XDND log if it was started with GDK_DEBUG=dnd
# (<files-log>), and the plug-in's "iPlug2 drop:" trace from <host-stderr>.
# Usage: drop-from-files.sh <out> <dir> <x> <y> <host-stderr> [files-log]
OUT="$1"; DIR="$2"; TX="$3"; TY="$4"; LOG="$5"; FLOG="${6:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
N=$(xdotool search --onlyvisible --name "^$(basename "$DIR")\$" | tail -1)
[ -n "$N" ] || { log "no Files window for $DIR"; exit 1; }
log "== window chain under $TX,$TY (deepest first)"; bash "$HERE/drop-chain.sh" "$TX" "$TY" | tee -a "$R"
B=$(grep -a -c 'iPlug2 drop' "$LOG" 2>/dev/null); FB=0; [ -n "$FLOG" ] && FB=$(wc -l < "$FLOG")
wmctrl -i -a "$N"; sleep 0.8
eval "$(xdotool getwindowgeometry --shell "$N")"
IX=$((X + 150)); IY=$((Y + 175))   # the first icon of a 482-wide grid view
import -window root "$OUT/before.png"
xdotool mousemove $IX $IY; sleep 0.3; xdotool mousedown 1; sleep 0.2
for i in 1 2 3 4 5 6 7 8 9 10; do xdotool mousemove $((IX + (TX - IX) * i / 10)) $((IY + (TY - IY) * i / 10)); sleep 0.1; done
sleep 0.5; xdotool mousemove $((TX + 2)) $((TY + 1)); sleep 0.5
import -window root "$OUT/over-target.png"
xdotool mouseup 1; sleep 2.5
import -window root "$OUT/after.png"
log "== plug-in drop trace (new lines)"; grep -a 'iPlug2 drop' "$LOG" 2>/dev/null | tail -n +$((B + 1)) | tee -a "$R"
[ -n "$FLOG" ] && { log "== Files XDND (new lines)"; tail -n +$((FB + 1)) "$FLOG" | grep -a -E 'Xdnd|Entering|Leaving|Sending|drop|Finish' | head -30 | tee -a "$R"; }
timeout 8 xdpyinfo >/dev/null && log X-OK
