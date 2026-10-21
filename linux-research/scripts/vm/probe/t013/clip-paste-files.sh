# T-020 item 3 on a real desktop: LinuxClipboard::SetFilePath (evidence/t020/clipboard/clipboard-file-test, built in
# Docker) owns CLIPBOARD with <file>; Ctrl+V in the GNOME Files window showing <dst> must copy the file there.
# Usage: clip-paste-files.sh <out> <clipboard-file-test> <file> <dst>   (Files already showing <dst>)
OUT="$1"; T="$2"; F="$3"; DST="$4"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
N=$(xdotool search --onlyvisible --name "^$(basename "$DST")\$" | tail -1)
[ -n "$N" ] || { log "no Files window"; exit 1; }
log "before: $(ls "$DST" | tr '\n' '|')"
"$T" "$F" 12 > "$OUT/owner.txt" 2>&1 &
sleep 1
for t in TARGETS text/uri-list x-special/gnome-copied-files; do log "$t: $(timeout 3 xclip -selection clipboard -t $t -o 2>&1 | tr '\r\n' '  ')"; done
wmctrl -i -a "$N"; sleep 0.5
eval "$(xdotool getwindowgeometry --shell "$N")"
xdotool mousemove $((X + WIDTH / 2)) $((Y + HEIGHT - 60)) mousedown 1; sleep 0.08; xdotool mouseup 1; sleep 0.5
xdotool key ctrl+v; sleep 3
import -window root "$OUT/after-paste.png"
log "after: $(ls "$DST" | tr '\n' '|')"
wait
cat "$OUT/owner.txt" >> "$R"
timeout 8 xdpyinfo >/dev/null && log X-OK
