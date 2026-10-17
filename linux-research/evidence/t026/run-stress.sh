#!/usr/bin/env bash
# N4 (FL Studio crash class): filter + mix automation sweeps with 0-frame blocks at every rate x os,
# Release and ASan; VST3 validator x20, ASan x3; clap-validator. Runs INSIDE the container (dock.sh).
# Usage: run-stress.sh <release-builddir> <asan-builddir|-> <outdir> [quick]
set -uo pipefail
R="$1/out"; A="$2/out"; O="$3"; QUICK="${4:-}"; mkdir -p "$O"
E="$(cd "$(dirname "$0")" && pwd)"
INC="$E/../../../Dependencies/IPlug/CLAP_SDK/include"
g++ -std=c++17 -O2 -DT026_NO_ALLOC_HOOK -I"$INC" "$E/render-t026.cpp" -o "$O/render-t026" -ldl || exit 1
ASAN_LIB="$(gcc -print-file-name=libasan.so)"
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=0

# Every filter param and both mix params swept every block; flt mix crosses 0 (setFilterMix's re-init).
FLT="sweep=10:0:1/11:0:1/12:0:2:i/13:0:4:i/14:0:1/5:0:1/6:0:1/7:0:1/4:0:1:i"
ON="p3=3 p18=0.7 p21=2 p22=0.5 p23=0.5"
renders() { # <clap> <tag> [env-prefix]
  local clap="$1" tag="$2" pre="${3:-}" crashes=0 nonfinite=0 n=0
  for sr in 44100 48000 88200 96000 192000; do
    for os in 0 1 2 3; do
      for blocks in "512" "64,1,1024,333"; do
        for z in 1 3; do for ZN in 0 1; do
          n=$((n + 1))
          line="$(env $pre timeout 600 "$O/render-t026" "$clap" - sr=$sr frames=$((sr / 2)) p15=$os blocks=$blocks \
                  zeroevery=$z zeronull=$ZN sweepframes=$((sr / 8)) $ON $FLT 2>&1 | grep -v 'event loop')"; rc=$?
          echo "$tag sr=$sr os=$os blocks=$blocks zeroevery=$z zeronull=$ZN rc=$rc :: $(echo "$line" | tail -3 | tr '\n' ' ' | cut -c1-400)"
          [ $rc -ne 0 ] && [ $rc -ne 3 ] && crashes=$((crashes + 1))
          [ $rc -eq 3 ] && nonfinite=$((nonfinite + 1))
          echo "$line" | grep -q "ERROR: AddressSanitizer" && crashes=$((crashes + 1))
        done
        done
      done
    done
  done
  echo "SUMMARY $tag: $n renders, $crashes crashed/ASan, $nonfinite with non-finite output"
}

renders "$R/Sand.clap" release | tee "$O/renders-release.txt" | grep SUMMARY
[ "$2" != "-" ] && renders "$A/Sand.clap" asan "LD_PRELOAD=$ASAN_LIB" | tee "$O/renders-asan.txt" | grep SUMMARY
[ -n "$QUICK" ] && exit 0

crashes=0; : > "$O/vst3-x20.txt"
for i in $(seq 1 20); do
  timeout 600 validator -e "$R/Sand.vst3" > "$O/vst3-run-$i.txt" 2>&1; rc=$?
  [ $rc -ne 0 ] && crashes=$((crashes + 1))
  echo "run $i exit=$rc $(grep -E 'Result: [0-9]+ tests passed' "$O/vst3-run-$i.txt" | tail -1)" >> "$O/vst3-x20.txt"
done
echo "vst3 validator -e x20: $crashes non-zero exits" | tee -a "$O/vst3-x20.txt"
if [ "$2" != "-" ]; then
  for i in 1 2 3; do LD_PRELOAD="$ASAN_LIB" timeout 900 validator -e "$A/Sand.vst3" > "$O/vst3-asan-$i.txt" 2>&1; done
  echo "vst3 asan x3: $(cat "$O"/vst3-asan-*.txt | grep -c 'ERROR: AddressSanitizer') ASan errors"
  LD_PRELOAD="$ASAN_LIB" timeout 900 clap-validator validate "$A/Sand.clap" > "$O/clap-asan.txt" 2>&1
  echo "clap asan: $(grep -c 'ERROR: AddressSanitizer' "$O/clap-asan.txt") ASan errors; $(grep -E 'tests run' "$O/clap-asan.txt" | tail -1)"
fi
timeout 900 clap-validator validate "$R/Sand.clap" > "$O/clap-validator.txt" 2>&1
echo "clap-validator: $(grep -E 'tests run' "$O/clap-validator.txt" | tail -1)"
grep -E "^\s+- .*FAILED|FAILED" "$O/clap-validator.txt" | head -5
