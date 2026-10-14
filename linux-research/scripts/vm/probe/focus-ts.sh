#!/usr/bin/env bash
# Drive focus-ts-probe.c under the session's window manager: per mode, N clicks into the "editor" child, each after a
# click on the other top-level. Prints where the focus is 50 ms and 600 ms after each release.
# Usage: focus-ts.sh <out> [trials=5] [modes...]
# Env: HOLD (s, default 0.08) between press and release of the editor click. xdotool's `click` sends both in the
#      same millisecond, which ties the X server's focus timestamps; a human click holds ~50-150 ms.
set -uo pipefail
OUT="$1"; N="${2:-5}"; shift 2 2>/dev/null || shift $#
MODES=("$@"); [ ${#MODES[@]} -gt 0 ] || MODES=(none now-current now-ts delay-current delay-ts)
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
gcc -O1 -o /tmp/focus-ts-probe "$HERE/focus-ts-probe.c" -lX11 || exit 1
wm=$(wmctrl -m | awk -F': ' '/^Name/{print $2}')
echo "window manager: $wm, press-release hold ${HOLD:-0.08} s" | tee -a "$R"
for m in "${MODES[@]}"; do
  /tmp/focus-ts-probe "$m" $((N * 3 + 6)) > "$OUT/probe-$m.txt" 2>&1 &
  P=$!; sleep 1.5
  T=$(xdotool search --name '^focus-ts-probe T$' | head -1); U=$(xdotool search --name '^focus-ts-probe U$' | head -1)
  for i in $(seq "$N"); do
    eval "$(xdotool getwindowgeometry --shell "$U")"; xdotool mousemove $((X + 50)) $((Y + 50)) click 1; sleep 0.8
    eval "$(xdotool getwindowgeometry --shell "$T")"; xdotool mousemove $((X + 200)) $((Y + 150)) mousedown 1; sleep "${HOLD:-0.08}"; xdotool mouseup 1; sleep 1.2
  done
  kill "$P" 2>/dev/null; wait "$P" 2>/dev/null
  grep '^RESULT' "$OUT/probe-$m.txt" | awk '{print $2, $4, $5}' | sort | uniq -c | tee -a "$R"
done
