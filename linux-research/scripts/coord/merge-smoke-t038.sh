#!/usr/bin/env bash
# T-038 smoke (VST3 fallback-thread notifications deferred to the host UI thread), run from the branch worktree before the merge:
# IPlugControls P3b steps, Gneiss and Sand steps in REAPER, CLAP + VST3, built against linux/T-038.
L=/Volumes/Storage/code/IPlug2-T-038; cd $L/linux-research
scripts/dock.sh -- scripts/build-cmake.sh .. build/t038m-ex IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T build/t038m-$p $p-clap $p-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
done
scripts/gui-up.sh t038m >/dev/null
for f in clap vst3; do
  GUI=t038m scripts/run-gui-test.sh build/t038m-ex/out/IPlugControls.$f evidence/merge-smoke/t038m-controls-$f scripts/steps/iplugcontrols-p3b.txt t038m 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec t038m pkill -x reaper
  for p in Gneiss Sand; do lp=$(echo $p | tr A-Z a-z)
    GUI=t038m scripts/run-gui-test.sh build/t038m-$p/out/$p.$f evidence/merge-smoke/t038m-$lp-$f scripts/steps/$lp.txt t038m 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== $lp $f"; docker exec t038m pkill -x reaper
  done
done
docker rm -f t038m >/dev/null; echo ALL-DONE
