#!/usr/bin/env bash
# Coordinator merge smoke for T-012 (trial-merged hvoya-linux).
cd /Volumes/Storage/code/IPlug2-linux/linux-research
E=evidence/merge-smoke; L=/Volumes/Storage/code/IPlug2-linux
DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Sand-linux /Volumes/Storage/code/hvoya/Sand/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Sand-linux build/m012-sand Sand-clap Sand-vst3 Sand-app 2>&1 | grep -E " error|RESULT|FAILED"
DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Gneiss-linux /Volumes/Storage/code/hvoya/Gneiss/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Gneiss-linux build/m012-gneiss Gneiss-clap 2>&1 | grep -E " error|RESULT|FAILED"
scripts/dock.sh -- scripts/build-cmake.sh .. build/m012 IPlugControls-clap 2>&1 | grep -E " error|RESULT|FAILED"
scripts/gui-up.sh m012 >/dev/null
for f in clap vst3; do GUI=m012 scripts/run-gui-test.sh build/m012-sand/out/Sand.$f $E/t012-sand-$f scripts/steps/sand.txt m012 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== sand $f"; done
docker exec m012 pkill -x reaper
GUI=m012 scripts/run-gui-test.sh build/m012-sand/out/Sand $E/t012-sand-app scripts/steps/sand-app.txt m012 2>&1 | grep -E "FAIL|DEAD|RESULT" | tail -3; echo "== sand app"
GUI=m012 scripts/run-gui-test.sh build/m012-gneiss/out/Gneiss.clap $E/t012-gneiss-clap scripts/steps/gneiss.txt m012 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== gneiss clap"
docker exec m012 pkill -x reaper
GUI=m012 scripts/run-gui-test.sh build/m012/out/IPlugControls.clap $E/t012-controls-clap scripts/steps/iplugcontrols-p3b.txt m012 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== controls clap"
docker rm -f m012 >/dev/null; echo ALL-DONE
