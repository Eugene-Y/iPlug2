#!/usr/bin/env bash
# N1: heap allocations inside Sand's CLAP process(), plus renders for the bit-exact comparison.
# Runs INSIDE the container (dock.sh). Usage: run-allocs.sh <release-builddir> <outdir>
set -uo pipefail
R="$1/out"; O="$2"; mkdir -p "$O/render"
E="$(cd "$(dirname "$0")" && pwd)"
g++ -std=c++17 -O2 -rdynamic -I"$E/../../../Dependencies/IPlug/CLAP_SDK/include" "$E/render-t026.cpp" -o "$O/render-t026" -ldl || exit 1

# Every DSP stage on: grind, dry HP grind, AM env + wave + gates + LP, filter half-mixed, mid/side, order 3.
ALL="p3=3 p14=0.5 p16=0.3 p17=1.6 p18=0.7 p21=2 p22=0.5 p23=0.5 p25=0.2 p26=0.2 p27=0.5"
# Filter cutoff/res/type/position/mix and joint mix swept every block (FL Studio report: filter + mix knob).
FLT="sweep=10:0:1/11:0:1/12:0:2:i/13:0:4:i/14:0:1/5:0:1"
run() { local name="$1"; shift; "$O/render-t026" "$R/Sand.clap" "$O/render/$name.raw" "$@" | sed "s/^/$name: /"; }
{
  for os in 0 1 2 3; do
    for sr in 44100 48000 96000 192000; do
      run "default-sr$sr-os$os" sr=$sr frames=$sr p15=$os
      run "all-sr$sr-os$os-blocks" sr=$sr frames=$sr p15=$os blocks=1024,64,333,512,1 $ALL
    done
    run "all-48k-os$os-f64-inplace" f64=1 inplace=1 p15=$os $ALL
    run "all-48k-os$os-random-zero" block=0 zeroevery=4 seed=3 p15=$os $ALL
    run "sweep-48k-os$os-zero" blocks=256,32,1024 zeroevery=3 p15=$os $ALL $FLT
  done
  # os switched on the audio thread, every block (applyOversampling in ProcessBlock).
  run "sweep-os-48k" blocks=512 $ALL sweep=15:0:3:i sweepframes=12000
  # re-activation at another rate with a larger max block (host changed its buffer size).
  run "react-48k-to-96k" frames=192000 blocks=512,64 react=96000 sr2=96000 blocks2=4096,128 p15=2 $ALL
  run "react-44k-to-44k-os3" sr=44100 frames=88200 blocks=128 react=44100 blocks2=2048,7 p15=3 $ALL
} | tee "$O/allocs.txt"
( cd "$O/render" && sha256sum *.raw > ../render.sha256 )
awk '{for(i=1;i<=NF;i++) if($i ~ /^alloc-blocks-after-first=/){split($i,a,"="); s+=a[2]} } END {print "TOTAL alloc-blocks-after-first:", s+0}' "$O/allocs.txt" | tee -a "$O/allocs.txt"
