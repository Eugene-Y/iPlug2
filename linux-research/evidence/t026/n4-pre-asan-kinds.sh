#!/usr/bin/env bash
# N4: the kind and place of the first ASan report in each pre-T-024 stress render (same matrix as run-stress.sh).
# Runs INSIDE the container. Usage: n4-pre-asan-kinds.sh <asan-builddir> <render-t026 built with -DT026_NO_ALLOC_HOOK>
C="$1/out/Sand.clap"; R="$2"
ASAN_LIB="$(gcc -print-file-name=libasan.so)"
FLT="sweep=10:0:1/11:0:1/12:0:2:i/13:0:4:i/14:0:1/5:0:1/6:0:1/7:0:1/4:0:1:i"
ON="p3=3 p18=0.7 p21=2 p22=0.5 p23=0.5"
for sr in 44100 48000 88200 96000 192000; do for os in 0 1 2 3; do for blocks in "512" "64,1,1024,333"; do
  for z in 1 3; do for ZN in 0 1; do
    LD_PRELOAD="$ASAN_LIB" "$R" "$C" - sr=$sr frames=$((sr / 2)) p15=$os blocks=$blocks zeroevery=$z zeronull=$ZN \
      sweepframes=$((sr / 8)) $ON $FLT 2>&1 | grep -m2 -E "ERROR: AddressSanitizer|#0 " \
      | sed -E 's/.*ERROR: AddressSanitizer: ([a-z-]+).*/\1/; s/.*#0 0x[0-9a-f]+ in ([^ ]+).* ([^ ]+:[0-9]+).*/\1 \2/' | paste -sd' ' -
  done; done
done; done; done | sed 's#/Volumes/Storage/code/##' | sort | uniq -c
