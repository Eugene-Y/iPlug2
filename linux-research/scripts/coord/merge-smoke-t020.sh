#!/usr/bin/env bash
# Coordinator smoke after merging T-020 (IGraphicsLinux: GL3, clipboard file, appearance, drag-out):
# IPlugControls P3b steps, Gneiss and Sand steps in REAPER, CLAP + VST3, on the merged hvoya-linux.
L=/Volumes/Storage/code/IPlug2-linux; cd $L/linux-research
scripts/dock.sh -- scripts/build-cmake.sh .. build/m020-ex IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T build/m020-$p $p-clap $p-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
done
scripts/gui-up.sh m020 >/dev/null
for f in clap vst3; do
  GUI=m020 scripts/run-gui-test.sh build/m020-ex/out/IPlugControls.$f evidence/merge-smoke/t020-controls-$f scripts/steps/iplugcontrols-p3b.txt m020 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec m020 pkill -x reaper
  for p in Gneiss Sand; do lp=$(echo $p | tr A-Z a-z)
    GUI=m020 scripts/run-gui-test.sh build/m020-$p/out/$p.$f evidence/merge-smoke/t020-$lp-$f scripts/steps/$lp.txt m020 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== $lp $f"; docker exec m020 pkill -x reaper
  done
done
docker rm -f m020 >/dev/null; echo ALL-DONE
