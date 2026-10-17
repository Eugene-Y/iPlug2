#!/usr/bin/env bash
# Coordinator smoke after merging T-024 (hvoya-linux 05d2c8e5c, Sand/Gneiss linux-experiment).
cd /Volumes/Storage/code/IPlug2-linux/linux-research
E=evidence/merge-smoke; L=/Volumes/Storage/code/IPlug2-linux
DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Sand-linux /Volumes/Storage/code/hvoya/Sand/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Sand-linux build/m024-sand Sand-clap Sand-vst3 Sand-app 2>&1 | grep -E " error|RESULT|FAILED"
DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Gneiss-linux /Volumes/Storage/code/hvoya/Gneiss/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Gneiss-linux build/m024-gneiss Gneiss-clap Gneiss-vst3 2>&1 | grep -E " error|RESULT|FAILED"
mkdir -p $E/t024-validators
for p in build/m024-sand/out/Sand.clap build/m024-gneiss/out/Gneiss.clap; do n=$(basename $p .clap)
  scripts/dock.sh -- bash -c "timeout 600 clap-validator validate $p > $E/t024-validators/$n-clap.txt 2>&1; sed 's/\x1b\[[0-9;]*m//g' $E/t024-validators/$n-clap.txt | grep -E 'tests run|ERROR Test'"; echo "== clap-validator $n"; done
scripts/dock.sh -- bash -c "c=0; for i in \$(seq 10); do timeout 300 validator -e build/m024-sand/out/Sand.vst3 > $E/t024-validators/Sand-vst3-\$i.txt 2>&1 || c=\$((c+1)); done; echo crashes-or-fails=\$c; grep -h 'Result:' $E/t024-validators/Sand-vst3-*.txt | sort | uniq -c"; echo "== vst3 validator Sand x10"
scripts/gui-up.sh m024 >/dev/null
for f in clap vst3; do GUI=m024 scripts/run-gui-test.sh build/m024-sand/out/Sand.$f $E/t024-sand-$f scripts/steps/sand.txt m024 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== sand $f"; docker exec m024 pkill -x reaper; done
for f in clap vst3; do GUI=m024 scripts/run-gui-test.sh build/m024-gneiss/out/Gneiss.$f $E/t024-gneiss-$f scripts/steps/gneiss.txt m024 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== gneiss $f"; docker exec m024 pkill -x reaper; done
docker rm -f m024 >/dev/null; rm -rf build/m024-sand build/m024-gneiss; echo ALL-DONE
