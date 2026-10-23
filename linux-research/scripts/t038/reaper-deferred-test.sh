#!/usr/bin/env bash
# T-038: VST3 host notifications from the fallback thread (editor closed) reach REAPER only on its main thread.
# Runs INSIDE the GUI container. Needs a Debug IPlugEffect VST3 built with IPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY
# IPLUG_TEST_LATENCY_FROM_IDLE IPLUG_TEST_EDIT_FROM_IDLE IPLUG_TRACE_HOST_LOOP.
# Usage: reaper-deferred-test.sh <outdir> <IPlugEffect.vst3> [expect: deferred|offthread]
#   deferred (default): the fixed build. offthread: the unfixed build, to show the bug the test catches.
# Phases (editor closed unless said): idle (REAPER makes no call), poll (getParamNormalized every 0.5 s),
#   play (transport start), open (editor open: frame IRunLoop).
set -uo pipefail
OUT="$1"; PLUG="$2"; EXPECT="${3:-deferred}"
HERE="$(cd "$(dirname "$0")" && pwd)"; S="$HERE/.."
mkdir -p "$OUT"; FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
now() { date +%s.%N; }
rpr() { "$S/rpr.sh" "$1"; }
pdc() { rpr "local t=reaper.GetTrack(0,0); local ok,v=reaper.TrackFX_GetNamedConfigParm(t,0,'pdc'); out(v)"; }

"$S/reaper-up.sh" "$PLUG" >/dev/null
RPID=$(cat /tmp/reaper.pid); echo "REAPER pid $RPID (main lwp)" | tee "$OUT/phases.txt"
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,'VST3: IPlugEffect',false,-1))"
sleep 1
timeout -s INT 40 gdb -q -batch -p "$RPID" -ex "set pagination off" -ex "set breakpoint pending on" \
  -ex "source $HERE/host-calls.py" -ex continue </dev/null > "$OUT/gdb.txt" 2>&1 &
GDB=$!
sleep 4
phase() { echo "PHASE $1 $(now)" >> "$OUT/phases.txt"; }

phase idle; : > "$OUT/pdc-idle.txt"
for i in $(seq 6); do pdc >> "$OUT/pdc-idle.txt"; sleep 0.7; done
phase poll; : > "$OUT/pdc-poll.txt"
for i in $(seq 12); do
  rpr "local t=reaper.GetTrack(0,0); out(reaper.TrackFX_GetParamNormalized(t,0,0))" > /dev/null
  pdc >> "$OUT/pdc-poll.txt"; sleep 0.4
done
phase play; rpr "reaper.OnPlayButton(); out('play')" > /dev/null; sleep 3; rpr "reaper.OnStopButton(); out('stop')" > /dev/null
phase open; rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3); out('shown')" > /dev/null; sleep 4
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2); out('closed')" > /dev/null
phase end
wait $GDB
grep '^CALL\|IPLUG_TEST' "$OUT/gdb.txt" > "$OUT/calls.txt"
grep 'IPLUG_TEST\|IPLUG_HOSTLOOP' /tmp/reaper.log > "$OUT/trace.txt"

# Label each call with its phase.
python3 - "$OUT" "$RPID" > "$OUT/calls-by-phase.txt" <<'PY'
import sys
out, pid = sys.argv[1], int(sys.argv[2])
phases = [(float(l.split()[2]), l.split()[1]) for l in open(out + "/phases.txt") if l.startswith("PHASE")]
def phase_of(t):
    name = "setup"
    for start, p in phases:
        if t >= start: name = p
    return name
for line in open(out + "/calls.txt"):
    if not line.startswith("CALL"): continue
    f = line.split()
    t, lwp = float(f[1]), int(f[2].split("=")[1])
    print(phase_of(t), "main" if lwp == pid else "OFF-MAIN", " ".join(f[3:]))
PY
sort "$OUT/calls-by-phase.txt" | uniq -c | tee "$OUT/summary.txt"
echo "pdc idle: $(tr '\n' ' ' < "$OUT/pdc-idle.txt")" | tee -a "$OUT/summary.txt"
echo "pdc poll: $(tr '\n' ' ' < "$OUT/pdc-poll.txt")" | tee -a "$OUT/summary.txt"

grep -q 'installed host breakpoints' "$OUT/calls.txt" || fail "the host's IComponentHandler was never found"
grep -q 'PLUG SetLatency' "$OUT/calls-by-phase.txt" || fail "no SetLatency at all"
if [ "$EXPECT" = deferred ]; then
  grep -q 'OFF-MAIN HOST' "$OUT/calls-by-phase.txt" && fail "a host notification off REAPER's main thread"
  grep -q '^idle .* HOST' "$OUT/calls-by-phase.txt" && fail "idle: a host notification although REAPER made no call"
  grep -q '^poll main HOST restartComponent flags=8' "$OUT/calls-by-phase.txt" || fail "poll: no restartComponent(kLatencyChanged) on the main thread"
  grep -q '^poll main HOST performEdit' "$OUT/calls-by-phase.txt" || fail "poll: no performEdit on the main thread"
  [ "$(sort -u "$OUT/pdc-poll.txt" | wc -l)" -ge 2 ] || fail "poll: REAPER's pdc never followed the latency"
else
  grep -q 'OFF-MAIN HOST restartComponent' "$OUT/calls-by-phase.txt" || fail "expected the unfixed build's off-main restartComponent"
fi
grep -q '^play main PLUG getLatencySamples' "$OUT/calls-by-phase.txt" || fail "play: REAPER did not read the latency on its main thread"
grep -q '^open main PLUG SetLatency' "$OUT/calls-by-phase.txt" || fail "open: SetLatency not on the main thread with the editor open"
pkill -f '^/opt/REAPER/reaper' 2>/dev/null
if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
