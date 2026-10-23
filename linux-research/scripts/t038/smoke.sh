#!/usr/bin/env bash
# T-038 regression smoke (host side): Release builds against this worktree; host-loop test in REAPER (CLAP, VST3;
# plain and fallback-forced); IPlugControls P3b steps, Gneiss and Sand steps in REAPER, CLAP + VST3.
# Usage: smoke.sh <evidence dir> [container=t038]
set -u
L="$(cd "$(dirname "$0")/../../.." && pwd)"; cd "$L/linux-research"
E="$1"; C="${2:-t038}"; mkdir -p "$E"
B=build/t038s
CXXFLAGS="-DIPLUG_TEST_LATENCY_FROM_IDLE -DIPLUG_TEST_EDIT_FROM_IDLE -DIPLUG_TRACE_HOST_LOOP"
scripts/dock.sh -- scripts/build-cmake.sh .. $B-ex IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
scripts/dock.sh -- bash -c "CXXFLAGS='$CXXFLAGS' scripts/build-cmake.sh .. $B-trace IPlugEffect-clap IPlugEffect-vst3 IPlugControls-vst3" 2>&1 | grep -E ' error|RESULT|FAILED'
scripts/dock.sh -- bash -c "CXXFLAGS='$CXXFLAGS -DIPLUG_TEST_VST3_FRAME_RUNLOOP_ONLY' scripts/build-cmake.sh .. $B-forced IPlugEffect-vst3" 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T $B-$p $p-clap $p-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
done
docker inspect "$C" >/dev/null 2>&1 || scripts/gui-up.sh "$C" >/dev/null
hl() { # name expect-closed plugin "FMT: Name" class
  docker exec -e EXPECT_CLOSED="$2" -w "$PWD" -e DISPLAY=:99 "$C" bash -c 'export DBUS_SESSION_BUS_ADDRESS=$(cat /tmp/dbus.addr); exec "$@"' _ \
    scripts/host-loop-test.sh "$E/hostloop-$1" "$4" "$3" "$5" > "$E/hostloop-$1.log" 2>&1
  echo "== hostloop $1: $(cat "$E/hostloop-$1/RESULT.txt" 2>/dev/null)"
}
hl effect-clap main $B-trace/out/IPlugEffect.clap "CLAP: IPlugEffect" IPlugEffect
hl effect-vst3 main $B-trace/out/IPlugEffect.vst3 "VST3: IPlugEffect" IPlugEffect
hl controls-vst3 main $B-trace/out/IPlugControls.vst3 "VST3: IPlugControls" IPlugControls
hl effect-vst3-forced worker $B-forced/out/IPlugEffect.vst3 "VST3: IPlugEffect" IPlugEffect
for f in clap vst3; do
  GUI=$C scripts/run-gui-test.sh $B-ex/out/IPlugControls.$f $E/controls-p3b-$f scripts/steps/iplugcontrols-p3b.txt $C 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec $C pkill -x reaper
  for p in Gneiss Sand; do lp=$(echo $p | tr A-Z a-z)
    GUI=$C scripts/run-gui-test.sh $B-$p/out/$p.$f $E/$lp-$f scripts/steps/$lp.txt $C 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== $lp $f"; docker exec $C pkill -x reaper
  done
done
echo ALL-DONE
