#!/usr/bin/env bash
# P2 host-loop test in REAPER, editor mostly closed. Runs INSIDE the GUI container (gx.sh).
# Usage: host-loop-test.sh <outdir> "<FMT: Name>" <plugin-bundle> [PluginClass=Name]
# Best with a build that defines IPLUG_TRACE_HOST_LOOP (and, for IPlugEffect, IPLUG_TEST_LATENCY_FROM_IDLE).
# Phases (gdb records every IPlugAPIBase::OnTimer / <Class>::OnIdle hit with its thread and object):
#   closed  - instance added, editor never opened
#   open    - editor shown (the window is still a stub in P2)
#   reclose - editor closed again
#   two     - a second instance on track 2, editors closed
#   latency - REAPER's "pdc" of instance 1 sampled while OnIdle changes the latency (test build only)
#   churn   - 50x add + delete an instance on track 3; threads, fds and live host registrations compared
# FAIL when: any hit off the REAPER main thread; no hits in a phase; not 2 timer objects in "two";
#   an OFF-MAIN line in the trace; threads/fds/registrations grow over the churn; REAPER dies; LUA ERROR.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; NAME="${FX#*: }"; CLASS="${4:-$NAME}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
finish() {
  cp /tmp/reaper.log "$OUT/reaper-stderr.txt" 2>/dev/null
  if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
  pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
  [ ${#FAILS[@]} -eq 0 ]; exit $?
}
rpr() { local r; r=$("$HERE/rpr.sh" "$1"); echo "$r"; if grep -q 'LUA ERROR\|no output from lua' <<<"$r"; then fail "lua: $r"; fi; }
alive() { local st; st=$(ps -o stat= -p "$RPID" 2>/dev/null); if [[ -z "$st" || "$st" == Z* ]]; then fail "REAPER died at '$1'"; finish; fi; }

# <phase> <seconds> [expected objects]: hits per symbol/thread/object; every hit must be on the main thread.
hits() {
  local phase="$1" secs="$2" want="${3:-}"
  "$HERE/gdb-hits.sh" "$RPID" "$secs" 'iplug::IPlugAPIBase::OnTimer(iplug::Timer&)' "$CLASS::OnIdle()" > "$OUT/hits-$phase.txt"
  echo "--- $phase: hits by symbol, thread (main = lwp $RPID), object"
  sed -E 's/\(.*\)//' "$OUT/hits-$phase.txt" | sort | uniq -c | tee "$OUT/hits-$phase-summary.txt"
  local total off timers
  total=$(grep -c 'OnTimer' "$OUT/hits-$phase.txt")
  off=$(grep -vc "lwp=$RPID " "$OUT/hits-$phase.txt")
  timers=$(grep 'OnTimer' "$OUT/hits-$phase.txt" | sed -E 's/.*this=//' | sort -u | wc -l)
  [ "$total" -gt 0 ] || fail "$phase: no OnTimer hits in ${secs}s"
  [ "$off" -eq 0 ] || fail "$phase: $off hits off the main thread"
  grep -q 'OnIdle' "$OUT/hits-$phase.txt" || fail "$phase: no OnIdle hits"
  [ -z "$want" ] || [ "$timers" -eq "$want" ] || fail "$phase: OnTimer ran for $timers objects, expected $want"
}
census() { # threads fds
  echo "$(ls /proc/$RPID/task | wc -l) $(ls /proc/$RPID/fd | wc -l)"
}

"$HERE/reaper-up.sh" "$PLUG"
RPID=$(cat /tmp/reaper.pid)
echo "REAPER pid $RPID"

rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1))"
sleep 2; alive add
hits closed 4 1

rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3); out('shown')"
sleep 2; alive open
hits open 4 1

rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2); out('closed')"
sleep 2; alive reclose
hits reclose 4 1

rpr "reaper.InsertTrackAtIndex(1,true); local t=reaper.GetTrack(0,1); out('add2', reaper.TrackFX_AddByName(t,\"$FX\",false,-1))"
sleep 2; alive two
hits two 4 2

echo "--- latency: REAPER pdc of track 1 fx 0 (editor closed)"
: > "$OUT/latency.txt"
for i in $(seq 8); do
  rpr "local t=reaper.GetTrack(0,0); local ok,v=reaper.TrackFX_GetNamedConfigParm(t,0,'pdc'); out('pdc', v)" >> "$OUT/latency.txt"
  sleep 0.5
done
cat "$OUT/latency.txt"
grep 'IPLUG_TEST SetLatency' /tmp/reaper.log | tail -6 | tee "$OUT/latency-calls.txt"
if [ -s "$OUT/latency-calls.txt" ]; then
  [ "$(sort -u "$OUT/latency.txt" | wc -l)" -ge 2 ] || fail "latency: OnIdle changed it, REAPER's pdc never changed"
  grep -v "tid=$RPID " "$OUT/latency-calls.txt" && fail "latency: SetLatency called off the main thread"
  grep -q 'ui=1' "$OUT/latency-calls.txt" && fail "latency: the editor was open"
fi

echo "--- churn: 50x add/delete on track 3"
rpr "reaper.InsertTrackAtIndex(2,true); out('track3')"
sleep 1
read -r T0 F0 < <(census)
L0=$(grep -o 'live registrations in module=[0-9]*' /tmp/reaper.log | tail -1)
for i in $(seq 50); do
  rpr "local t=reaper.GetTrack(0,2); local i=reaper.TrackFX_AddByName(t,\"$FX\",false,-1); out('a',i)" > /dev/null
  sleep 0.15
  rpr "local t=reaper.GetTrack(0,2); out('d', reaper.TrackFX_Delete(t,0), reaper.TrackFX_GetCount(t))" > /dev/null
done
sleep 2; alive churn
read -r T1 F1 < <(census)
L1=$(grep -o 'live registrations in module=[0-9]*' /tmp/reaper.log | tail -1)
CREATED=$(grep -c ': backend=' /tmp/reaper.log)
echo "threads $T0 -> $T1, fds $F0 -> $F1, trace: '$L0' -> '$L1', backends installed in total: $CREATED" | tee "$OUT/churn.txt"
[ "$T1" -le "$T0" ] || fail "churn: threads grew $T0 -> $T1"
[ "$F1" -le "$F0" ] || fail "churn: fds grew $F0 -> $F1"
# the last instance destroyed reports what the module still has registered: the 2 remaining instances' timers
[ -z "$L1" ] || [ "${L1##*=}" -eq 2 ] || fail "churn: $L1 after the churn, expected 2 (one timer per remaining instance)"
hits after-churn 3 2

"$HERE/threads.sh" "$RPID" 8 > "$OUT/threads.txt" 2>&1
grep 'IPLUG_HOSTLOOP' /tmp/reaper.log > "$OUT/trace.txt"
grep -q 'OFF-MAIN' "$OUT/trace.txt" && fail "trace: a callback ran off the main thread"
alive end
finish
