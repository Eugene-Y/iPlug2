#!/usr/bin/env bash
# Coordinator merge smoke for T-006 (trial-merged hvoya-linux).
cd /Volumes/Storage/code/IPlug2-linux/linux-research
E=evidence/merge-smoke; L=/Volumes/Storage/code/IPlug2-linux
scripts/dock.sh -- scripts/build-cmake.sh .. build/m006 IPlugControls-clap IPlugEffect-clap 2>&1 | grep -E " error|RESULT|FAILED"
DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Gneiss-linux /Volumes/Storage/code/hvoya/Gneiss/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Gneiss-linux build/m006-gneiss Gneiss-clap 2>&1 | grep -E " error|RESULT|FAILED"
mkdir -p $E/t006-validators
for p in build/m006/out/IPlugControls.clap build/m006/out/IPlugEffect.clap build/m006-gneiss/out/Gneiss.clap; do
  n=$(basename $p .clap); scripts/dock.sh -- bash -c "timeout 600 clap-validator validate $p > $E/t006-validators/$n.txt 2>&1; grep -E 'tests run' $E/t006-validators/$n.txt | tail -1; grep -E '^ *- *state-|state-reproducibility' $E/t006-validators/$n.txt | head -6"; echo "== validator $n"
done
scripts/gui-up.sh m006 >/dev/null
for p in build/m006/out/IPlugControls.clap build/m006-gneiss/out/Gneiss.clap; do n=$(basename $p .clap); GUI=m006 scripts/run-gui-test.sh $p $E/t006-roundtrip-$n scripts/steps/state-roundtrip.txt m006 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== roundtrip $n"; docker exec m006 pkill -x reaper; done
GUI=m006 scripts/run-gui-test.sh build/m006/out/IPlugControls.clap $E/t006-controls-clap scripts/steps/iplugcontrols-p3b.txt m006 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== controls p3b"
docker exec m006 pkill -x reaper
GUI=m006 scripts/run-gui-test.sh build/m006-gneiss/out/Gneiss.clap $E/t006-gneiss-clap scripts/steps/gneiss.txt m006 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== gneiss steps"
docker rm -f m006 >/dev/null; rm -rf build/m006 build/m006-gneiss; echo ALL-DONE
