#!/usr/bin/env bash
# Coordinator smoke after merging T-026 + hvoya T21-5 (hvoya-linux 04cafbd05, Sand linux-experiment 87ccdbf).
cd /Volumes/Storage/code/IPlug2-linux/linux-research
E=evidence/merge-smoke; L=/Volumes/Storage/code/IPlug2-linux
DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Sand-linux /Volumes/Storage/code/hvoya/Sand/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Sand-linux build/m026-sand Sand-clap Sand-vst3 Sand-app 2>&1 | grep -E " error|RESULT|FAILED"
DOCK_MOUNTS="/Volumes/Storage/code/hvoya/Gneiss-linux /Volumes/Storage/code/hvoya/Gneiss/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh /Volumes/Storage/code/hvoya/Gneiss-linux build/m026-gneiss Gneiss-clap Gneiss-vst3 2>&1 | grep -E " error|RESULT|FAILED"
mkdir -p $E/t026-validators
for p in build/m026-sand/out/Sand.clap build/m026-gneiss/out/Gneiss.clap; do n=$(basename $p .clap)
  scripts/dock.sh -- bash -c "timeout 600 clap-validator validate $p > $E/t026-validators/$n-clap.txt 2>&1; sed 's/\x1b\[[0-9;]*m//g' $E/t026-validators/$n-clap.txt | grep -E 'tests run|ERROR Test'"; echo "== clap-validator $n"; done
scripts/dock.sh -- bash -c "c=0; for i in \$(seq 10); do timeout 300 validator -e build/m026-sand/out/Sand.vst3 > $E/t026-validators/Sand-vst3-\$i.txt 2>&1 || c=\$((c+1)); done; echo crashes-or-fails=\$c; grep -h 'Result:' $E/t026-validators/Sand-vst3-*.txt | sort | uniq -c"; echo "== vst3 validator Sand x10"
scripts/gui-up.sh m026 >/dev/null
for f in clap vst3; do GUI=m026 scripts/run-gui-test.sh build/m026-sand/out/Sand.$f $E/t026-sand-$f scripts/steps/sand.txt m026 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== sand $f"; docker exec m026 pkill -x reaper; done
for f in clap vst3; do GUI=m026 scripts/run-gui-test.sh build/m026-gneiss/out/Gneiss.$f $E/t026-gneiss-$f scripts/steps/gneiss.txt m026 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== gneiss $f"; docker exec m026 pkill -x reaper; done
docker rm -f m026 >/dev/null; rm -rf build/m026-sand build/m026-gneiss; echo ALL-DONE
