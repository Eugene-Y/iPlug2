#!/usr/bin/env bash
# T-034 smoke (NanoVG GL main-FBO stencil clear), run from the branch worktree before the merge:
# IPlugControls P3b steps, Gneiss and Sand steps in REAPER, CLAP + VST3, built against linux/T-034.
L=/Volumes/Storage/code/IPlug2-T-034; cd $L/linux-research
scripts/dock.sh -- scripts/build-cmake.sh .. build/t034s-ex IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T build/t034s-$p $p-clap $p-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
done
scripts/gui-up.sh t034s >/dev/null
for f in clap vst3; do
  GUI=t034s scripts/run-gui-test.sh build/t034s-ex/out/IPlugControls.$f evidence/merge-smoke/t034-controls-$f scripts/steps/iplugcontrols-p3b.txt t034s 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec t034s pkill -x reaper
  for p in Gneiss Sand; do lp=$(echo $p | tr A-Z a-z)
    GUI=t034s scripts/run-gui-test.sh build/t034s-$p/out/$p.$f evidence/merge-smoke/t034-$lp-$f scripts/steps/$lp.txt t034s 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== $lp $f"; docker exec t034s pkill -x reaper
  done
done
docker rm -f t034s >/dev/null; echo ALL-DONE
