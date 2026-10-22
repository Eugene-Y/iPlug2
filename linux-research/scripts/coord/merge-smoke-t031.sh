#!/usr/bin/env bash
# T-031 smoke (REAPER extensions on Linux; IGraphicsLinux clear+swap at open affects every editor), run from the branch worktree before the merge:
# IPlugControls P3b steps, Gneiss and Sand steps in REAPER, CLAP + VST3, built against linux/T-031.
L=/Volumes/Storage/code/IPlug2-T-031; cd $L/linux-research
scripts/dock.sh -- scripts/build-cmake.sh .. build/t031s-ex IPlugControls-clap IPlugControls-vst3 IPlugReaperExtension IPlugReaperExtensionWebUI 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T build/t031s-$p $p-clap $p-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
done
scripts/gui-up.sh t031s >/dev/null
for f in clap vst3; do
  GUI=t031s scripts/run-gui-test.sh build/t031s-ex/out/IPlugControls.$f evidence/merge-smoke/t031m-controls-$f scripts/steps/iplugcontrols-p3b.txt t031s 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec t031s pkill -x reaper
  for p in Gneiss Sand; do lp=$(echo $p | tr A-Z a-z)
    GUI=t031s scripts/run-gui-test.sh build/t031s-$p/out/$p.$f evidence/merge-smoke/t031m-$lp-$f scripts/steps/$lp.txt t031s 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== $lp $f"; docker exec t031s pkill -x reaper
  done
done
for e in IPlugReaperExtension IPlugReaperExtensionWebUI; do
  scripts/gx.sh t031s scripts/reaper-ext-test.sh evidence/merge-smoke/t031m-$e build/t031s-ex/out/reaper_$e.so 2>&1 | grep -E "FAIL|RESULT" | tail -3; echo "== $e"; docker exec t031s pkill -x reaper
done
docker rm -f t031s >/dev/null; echo ALL-DONE
