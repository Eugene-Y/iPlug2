#!/usr/bin/env bash
# mac: before/after renders (bit-exact), filter/mix sweeps with 0-frame blocks, VST3 validator x20 (Debug: asserts
# live), clap-validator. No ASan (it hangs on this Mac).
# Usage: mac-checks.sh <before.clap> <after.clap> <after-debug.vst3> <outdir> <clap-validator> <vst3-validator>
set -uo pipefail
B="$1"; A="$2"; V="$3"; O="$4"; CV="$5"; VV="$6"; mkdir -p "$O/before" "$O/after"
E="$(cd "$(dirname "$0")" && pwd)"
clang++ -std=c++17 -O2 -I"$E/../../../Dependencies/IPlug/CLAP_SDK/include" "$E/render-t026.cpp" -o "$O/render-t026" || exit 1

ALL="p3=3 p14=0.5 p16=0.3 p17=1.6 p18=0.7 p21=2 p22=0.5 p23=0.5 p25=0.2 p26=0.2 p27=0.5"
FLT="sweep=10:0:1/11:0:1/12:0:2:i/13:0:4:i/14:0:1/5:0:1/6:0:1/7:0:1/4:0:1:i"
pair() { local name="$1"; shift
  "$O/render-t026" "$B" "$O/before/$name.raw" "$@" 2>/dev/null | sed "s/^/before $name: /"
  "$O/render-t026" "$A" "$O/after/$name.raw" "$@" 2>/dev/null | sed "s/^/after  $name: /"; }
{
  for os in 0 1 2 3; do
    for sr in 44100 48000 88200 96000 192000; do
      pair "all-sr$sr-os$os" sr=$sr frames=$((sr / 2)) p15=$os blocks=1024,64,333,512 $ALL
      pair "sweep-sr$sr-os$os-zero" sr=$sr frames=$((sr / 2)) p15=$os blocks=512,64,1 zeroevery=3 zeronull=1 sweepframes=$((sr / 8)) $ALL $FLT
    done
    pair "f64-inplace-os$os" f64=1 inplace=1 p15=$os $ALL
  done
  pair "sweep-os" blocks=512 $ALL sweep=15:0:3:i sweepframes=12000
  pair "react-48k-to-96k" frames=192000 blocks=512,64 react=96000 sr2=96000 blocks2=4096,128 p15=2 $ALL
} > "$O/renders.txt"
echo "renders: $(grep -c '^after' "$O/renders.txt") pairs; non-zero non-finite: $(grep -v 'non-finite=0' "$O/renders.txt" | wc -l | tr -d ' ')"
( cd "$O/before" && shasum -a 256 *.raw ) > "$O/before.sha256"
( cd "$O/after" && shasum -a 256 *.raw ) > "$O/after.sha256"
if diff -q "$O/before.sha256" "$O/after.sha256" >/dev/null; then echo "bit-identical: all"; else
  echo "differ:"; diff "$O/before.sha256" "$O/after.sha256"; fi

crashes=0; : > "$O/vst3-x20.txt"
for i in $(seq 1 20); do
  "$VV" -e "$V" > "$O/vst3-run-$i.txt" 2>&1; rc=$?
  [ $rc -ne 0 ] && crashes=$((crashes + 1))
  echo "run $i exit=$rc $(grep -E 'Result: [0-9]+ tests passed' "$O/vst3-run-$i.txt" | tail -1)" >> "$O/vst3-x20.txt"
done
echo "vst3 (Debug) validator -e x20: $crashes non-zero exits" | tee -a "$O/vst3-x20.txt"
"$CV" validate "$A" > "$O/clap-validator.txt" 2>&1
echo "clap-validator: $(grep -E 'tests run' "$O/clap-validator.txt" | tail -1)"
grep -B1 -A3 "FAILED" "$O/clap-validator.txt" | grep -E "^\s+- |FAILED" | head -10
