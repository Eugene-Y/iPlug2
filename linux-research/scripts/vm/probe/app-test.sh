#!/usr/bin/env bash
# Standalone APP on the real desktop, with its default audio I/O (ALSA "default" = PipeWire here) unless
# APP_ARGS=--no-io: render, the mouse-only step script, threads, whether PipeWire sees an audio stream.
# Usage: app-test.sh <out> <app-binary> [steps]
set -uo pipefail
OUT="$1"; APP="$2"; STEPS="${3:-}"; NAME=$(basename "$APP")
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"; : > "$OUT/xact-fails.txt"
log() { echo "$*" | tee -a "$R"; }
pkill -x "$NAME" 2>/dev/null; sleep 0.5
( cd "$(dirname "$APP")" && setsid "$APP" ${APP_ARGS:-} > "$HERE/../$OUT/stderr.txt" 2>&1 < /dev/null & )
sleep 5
PID=$(pgrep -x "$NAME" | head -1)
[ -n "$PID" ] || { log "APP not running; stderr:"; cat "$OUT/stderr.txt" | tee -a "$R"; exit 1; }
TOP=$(for w in $(xdotool search --onlyvisible --name "^$NAME\$"); do xprop -id "$w" WM_CLASS | grep -q "\"$NAME\"" && echo "$w"; done | head -1)
[ -n "$TOP" ] || { log "no top-level window '$NAME'"; exit 1; }
# plugin view = largest descendant of the app's top-level window
read -r OX OY OW OH < <(xwininfo -id "$TOP" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }')
log "pid $PID, view ${OW}x${OH} at $OX,$OY"
import -window root "$OUT/a01-open.png"
sd=$(convert "$OUT/a01-open.png" -crop "${OW}x${OH}+${OX}+${OY}" +repage -colorspace Gray -format '%[fx:standard_deviation]' info:)
log "render stddev $sd"
wpctl status 2>/dev/null | grep -iE "$NAME|alsa_playback|ALSA plug-in" | sed 's/^/pipewire: /' | tee -a "$R"
pw-dump 2>/dev/null | grep -o "\"application.name\": \"[^\"]*\"" | sort -u | sed 's/^/pw-client: /' | tee -a "$R"
if [ -n "$STEPS" ]; then
  (echo "origin $OX $OY"; sed "s/__PROC__/$PID/" "$STEPS") | OUT="$OUT" "$HERE/xact.sh" > "$OUT/xact.log" 2>&1
  grep -E "changed|FAIL|DEAD|ALIVE" "$OUT/xact.log" | tee -a "$R"
fi
"$HERE/threads.sh" "$PID" 10 > "$OUT/threads.txt" 2>&1
log "threads: $(sed -n '/^--- threads/,/^--- backtraces/p' "$OUT/threads.txt" | grep -vE '^---' | tr -s ' ' | tr '\n' ';')"
ps -o stat= -p "$PID" >/dev/null && log "APP alive at end" || log "APP DEAD at end"
wmctrl -i -c "$TOP"; sleep 2; pgrep -x "$NAME" >/dev/null && { log "APP still running after WM close; killing"; pkill -x "$NAME"; } || log "APP exited on WM close"
tail -5 "$OUT/stderr.txt" | sed 's/^/stderr: /' | tee -a "$R"
