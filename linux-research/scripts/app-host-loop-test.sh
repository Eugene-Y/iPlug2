#!/usr/bin/env bash
# P2: the standalone APP's OnTimer/OnIdle must run on its main thread. Runs INSIDE the GUI container.
# Usage: app-host-loop-test.sh <outdir> <app-binary> [PluginClass=basename]
set -uo pipefail
OUT="$1"; APP="$2"; CLASS="${3:-$(basename "$APP")}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
pkill -x "$(basename "$APP")" 2>/dev/null; sleep 0.5
"$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
PID=$!
sleep 4
if ! kill -0 "$PID" 2>/dev/null; then fail "APP exited early: $(tail -3 "$OUT/stderr.txt")"; else
  "$HERE/gdb-hits.sh" "$PID" 4 'iplug::IPlugAPIBase::OnTimer(iplug::Timer&)' "$CLASS::OnIdle()" > "$OUT/hits.txt"
  echo "--- hits by symbol, thread (main = lwp $PID), object"
  sed -E 's/\(.*\)//' "$OUT/hits.txt" | sort | uniq -c | tee "$OUT/hits-summary.txt"
  grep -q OnTimer "$OUT/hits.txt" || fail "no OnTimer hits"
  grep -q OnIdle "$OUT/hits.txt" || fail "no OnIdle hits"
  OFF=$(grep -vc "lwp=$PID " "$OUT/hits.txt"); [ "$OFF" -eq 0 ] || fail "$OFF hits off the main thread"
  "$HERE/threads.sh" "$PID" 8 > "$OUT/threads.txt" 2>&1
  kill "$PID"; sleep 1; kill -9 "$PID" 2>/dev/null
fi
grep 'IPLUG_HOSTLOOP' "$OUT/stderr.txt" > "$OUT/trace.txt"
grep -q 'OFF-MAIN' "$OUT/trace.txt" && fail "trace: a callback ran off the main thread"
if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
[ ${#FAILS[@]} -eq 0 ]
