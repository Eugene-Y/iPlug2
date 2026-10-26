#!/usr/bin/env bash
# T-030 regression smoke, run from the branch worktree: IPlugControls P3b, Gneiss and Sand steps in REAPER, CLAP + VST3
# (Xvfb has no touchscreen: this is the path without XInput touches).
L=/Volumes/Storage/code/IPlug2-T-030; cd $L/linux-research
scripts/dock.sh -- scripts/build-cmake.sh .. build/t030-ex IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T build/t030-$p $p-clap $p-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
done
scripts/gui-up.sh t030s >/dev/null
for f in clap vst3; do
  GUI=t030s scripts/run-gui-test.sh build/t030-ex/out/IPlugControls.$f evidence/t030/docker-controls-$f scripts/steps/iplugcontrols-p3b.txt t030s 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec t030s pkill -x reaper
  for p in Gneiss Sand; do lp=$(echo $p | tr A-Z a-z)
    GUI=t030s scripts/run-gui-test.sh build/t030-$p/out/$p.$f evidence/t030/docker-$lp-$f scripts/steps/$lp.txt t030s 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== $lp $f"; docker exec t030s pkill -x reaper
  done
done
docker rm -f t030s >/dev/null; echo ALL-DONE
