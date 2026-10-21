#!/usr/bin/env bash
# Coordinator smoke for the T-023 merge (IPlugCLAP: preset rescan, flush only with clap.params), on the merge tree:
# IPlugControls P3b steps, Gneiss and Sand steps in REAPER, CLAP + VST3, on the merged hvoya-linux; clap-validator on the Gneiss and Sand CLAPs.
L=/Volumes/Storage/code/IPlug2-linux; cd $L/linux-research
scripts/dock.sh -- scripts/build-cmake.sh .. build/m023-ex IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T build/m023-$p $p-clap $p-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
done
scripts/dock.sh -- scripts/validate.sh evidence/merge-smoke/t023m-validators build/m023-Gneiss/out/Gneiss.clap build/m023-Sand/out/Sand.clap build/m023-ex/out/IPlugControls.clap
scripts/gui-up.sh m023 >/dev/null
for f in clap vst3; do
  GUI=m023 scripts/run-gui-test.sh build/m023-ex/out/IPlugControls.$f evidence/merge-smoke/t023m-controls-$f scripts/steps/iplugcontrols-p3b.txt m023 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec m023 pkill -x reaper
  for p in Gneiss Sand; do lp=$(echo $p | tr A-Z a-z)
    GUI=m023 scripts/run-gui-test.sh build/m023-$p/out/$p.$f evidence/merge-smoke/t023m-$lp-$f scripts/steps/$lp.txt m023 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== $lp $f"; docker exec m023 pkill -x reaper
  done
done
docker rm -f m023 >/dev/null; echo ALL-DONE
