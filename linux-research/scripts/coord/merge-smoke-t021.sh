#!/usr/bin/env bash
# Coordinator smoke after merging T-021 (hvoya-linux 631f78208): package and smoke Sand + Gneiss.
L=/Volumes/Storage/code/IPlug2-linux; R=$L/linux-research; D=$R/build/m021-dist; E=$R/evidence/merge-smoke
cd $L
Scripts/linux/package.sh /Volumes/Storage/code/hvoya/Sand-linux --arch aarch64 --arch x86_64 --out $D --clean 2>&1 | tail -4; echo "== package Sand"
IPLUG2_DIR=$L /Volumes/Storage/code/hvoya/Gneiss-linux/scripts/makedist-linux.sh --only paid --arch aarch64 --out $D --clean 2>&1 | tail -4; echo "== package Gneiss"
ls $D/*.tar.gz | grep -v debug
cd $R
for a in $D/Sand_*_linux_aarch64.tar.gz $D/Gneiss_*_linux_aarch64_paid.tar.gz; do n=$(basename $a .tar.gz)
  scripts/package-smoke.sh $a $E/t021-$n iplug2-rt:ubuntu22.04 2>&1 | grep -E 'FAIL|RESULT' | tail -6; echo "== smoke $n"; done
a=$(ls $D/Sand_*_linux_x86_64.tar.gz); scripts/package-smoke.sh $a $E/t021-$(basename $a .tar.gz) iplug2-rt:ubuntu22.04-amd64 2>&1 | grep -E 'FAIL|RESULT' | tail -6; echo "== smoke Sand x86_64"
echo ALL-DONE
