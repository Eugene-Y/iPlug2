#!/usr/bin/env bash
# Validators, VST3 x20, ASan and offline renders for one Sand build pair. Runs INSIDE the container (dock.sh).
# Usage: run-checks.sh <release-builddir> <asan-builddir> <outdir>
set -uo pipefail
R="$1/out"; A="$2/out"; O="$3"; mkdir -p "$O/render"
E="$(cd "$(dirname "$0")" && pwd)"
g++ -std=c++17 -O2 -I"$E/../../../Dependencies/IPlug/CLAP_SDK/include" "$E/render-clap.cpp" -o "$O/render-clap" -ldl || exit 1

timeout 900 clap-validator validate "$R/Sand.clap" > "$O/clap-validator.txt" 2>&1
echo "clap-validator: $(grep -E 'tests run' "$O/clap-validator.txt" | tail -1)"

crashes=0
for i in $(seq 1 20); do
  timeout 600 validator -e "$R/Sand.vst3" > "$O/vst3-run-$i.txt" 2>&1; rc=$?
  [ $rc -ne 0 ] && crashes=$((crashes + 1))
  echo "run $i exit=$rc $(grep -E 'Result: [0-9]+ tests passed' "$O/vst3-run-$i.txt" | tail -1)" >> "$O/vst3-x20.txt"
done
echo "vst3 validator -e x20: $crashes non-zero exits" | tee -a "$O/vst3-x20.txt"

ASAN_LIB="$(gcc -print-file-name=libasan.so)"
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=0
for i in 1 2 3; do
  LD_PRELOAD="$ASAN_LIB" timeout 900 validator -e "$A/Sand.vst3" > "$O/vst3-asan-$i.txt" 2>&1
done
echo "vst3 asan: $(cat "$O"/vst3-asan-*.txt | grep -c 'ERROR: AddressSanitizer') ASan errors in 3 runs"
LD_PRELOAD="$ASAN_LIB" timeout 900 clap-validator validate "$A/Sand.clap" > "$O/clap-asan.txt" 2>&1
echo "clap asan: $(grep -c 'ERROR: AddressSanitizer' "$O/clap-asan.txt") ASan errors; $(grep -E 'tests run' "$O/clap-asan.txt" | tail -1)"

render() { local name="$1"; shift; "$O/render-clap" "$R/Sand.clap" "$O/render/$name.raw" "$@" | sed "s/^/$name: /"; }
{
  render default-48k-512-f32
  render default-48k-512-f64 f64=1
  render default-48k-512-f64-inplace f64=1 inplace=1
  render default-44k-64-f32 sr=44100 block=64
  render default-96k-1000-f64 sr=96000 block=1000 f64=1
  render random-blocks-48k-f32 block=0 seed=7
  render order3-os2-48k-f32 p3=3 p15=1
  render grind-am-48k-f64 f64=1 p18=0.7 p22=0.5 p23=0.5
  for sr in 1000 22050 192000 384000 768000 767999.5; do render "default-${sr}" sr=$sr frames=$(( ${sr%.*} )) ; done
} | tee "$O/render.txt"
( cd "$O/render" && sha256sum *.raw > ../render.sha256 )
