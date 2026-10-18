#!/usr/bin/env bash
# Coordinator smoke after merging T-028 (Gneiss master c6f4442, linux-experiment f799cb3; hvoya-linux 9f9afa0e9):
# Gneiss packages (clap-validator with an empty known-failures list), REAPER steps CLAP + VST3.
L=/Volumes/Storage/code/IPlug2-linux; R=$L/linux-research; D=$R/build/m028-dist; E=$R/evidence/merge-smoke
G=/Volumes/Storage/code/hvoya/Gneiss-linux
IPLUG2_DIR=$L $G/scripts/makedist-linux.sh --only paid --arch aarch64 --arch x86_64 --out $D --clean 2>&1 | tail -4; echo "== package Gneiss"
ls $D/*.tar.gz | grep -v debug
cd $R
a=$(ls $D/Gneiss_*_linux_aarch64_paid.tar.gz); scripts/package-smoke.sh $a $E/t028-$(basename $a .tar.gz) iplug2-rt:ubuntu22.04 2>&1 | grep -E 'FAIL|RESULT' | tail -6; echo "== smoke Gneiss aarch64"
a=$(ls $D/Gneiss_*_linux_x86_64_paid.tar.gz); scripts/package-smoke.sh $a $E/t028-$(basename $a .tar.gz) iplug2-rt:ubuntu22.04-amd64 2>&1 | grep -E 'FAIL|RESULT' | tail -6; echo "== smoke Gneiss x86_64"
DOCK_MOUNTS="$G /Volumes/Storage/code/hvoya/Gneiss/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $G build/m028-gneiss Gneiss-clap Gneiss-vst3 2>&1 | grep -E " error|RESULT|FAILED"
scripts/gui-up.sh m028 >/dev/null
for f in clap vst3; do GUI=m028 scripts/run-gui-test.sh build/m028-gneiss/out/Gneiss.$f $E/t028-gneiss-$f scripts/steps/gneiss.txt m028 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3; echo "== gneiss $f"; docker exec m028 pkill -x reaper; done
docker rm -f m028 >/dev/null; echo ALL-DONE
