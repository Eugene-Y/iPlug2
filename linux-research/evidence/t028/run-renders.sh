#!/usr/bin/env bash
# T-028: Gneiss renders for the bit-exact comparison, the host-input check and the allocation counter.
# Runs INSIDE the container (dock.sh) or on mac (no allocation counter there).
# Usage: run-renders.sh <render-t028 binary> <Gneiss.clap> <outdir> <reference .state>
# Every render loads the same reference state first: a fresh instance picks a random starter preset and
# random morph points, so two fresh instances of one build already differ.
set -uo pipefail
R="$1"; CLAP="$2"; O="$3"; REF="$4"; mkdir -p "$O/render" "$O/state"

# Gneiss CLAP ids: 11 main hz, 13 main res, 28 oversampling (0..3), 29 mix, 30 gain, 31 gain on dry,
# 56 morph active (default on), 57/58 morph X/Y. Doubles take normalized values, enums plain ones.
AUTO="sweep=29:0:1/30:0.2:0.8 p31=0.5"
CUT="p56=0 sweep=11:0.2:0.9/13:0.1:0.9"
MORPH="p56=1 sweep=57:0:1/58:0:1"
run() { local name="$1"; shift; "$R" "$CLAP" "$O/render/$name.raw" load="$REF" state="$O/state/$name.state" "$@" 2>/dev/null | sed "s/^/$name: /"; }
{
  for os in 0 1 2 3; do
    run "os$os-f32-512"            p28=$os frames=48000
    run "os$os-f64-512"            p28=$os frames=48000 f64=1
    run "os$os-f64-512-inplace"    p28=$os frames=48000 f64=1 inplace=1
    run "os$os-f32-512-inplace"    p28=$os frames=48000 inplace=1
    for b in 1 64 513 4096; do
      run "os$os-f64-b$b"          p28=$os frames=48000 f64=1 blocks=$b
    done
    run "os$os-f64-mixgain"        p28=$os frames=96000 f64=1 blocks=512,64,333 $AUTO
    run "os$os-f32-mixgain-inplace" p28=$os frames=96000 inplace=1 blocks=4096,1,513 $AUTO
    run "os$os-f64-cutoff-auto"    p28=$os frames=96000 f64=1 blocks=256,1000 $CUT
    run "os$os-f64-morph-xy"       p28=$os frames=96000 f64=1 blocks=512,77 $MORPH
    run "os$os-random-zero"        p28=$os frames=96000 f64=1 block=0 zeroevery=4 seed=3 $AUTO
  done
  run "os-switching"               frames=96000 f64=1 blocks=512 sweep=28:0:3:i sweepframes=12000
  run "react-48k-to-96k"           frames=192000 f64=1 blocks=512,64 react=96000 sr2=96000 blocks2=4096,128 p28=2 $AUTO
} | tee "$O/renders.txt"
( cd "$O/render" && (sha256sum *.raw 2>/dev/null || shasum -a 256 *.raw) > ../render.sha256 )
( cd "$O/state" && (sha256sum *.state 2>/dev/null || shasum -a 256 *.state) > ../state.sha256 )
awk '{for(i=1;i<=NF;i++){ if($i ~ /^alloc-blocks-after-first=/){split($i,a,"="); s+=a[2]} if($i ~ /^allocs=/){split($i,b,"="); t+=b[2]} if($i ~ /^input-changed=/){split($i,c,"="); u+=c[2]} if($i ~ /^non-finite=/){split($i,d,"="); v+=d[2]} } }
     END {print "TOTAL allocs:", t+0, " alloc-blocks-after-first:", s+0, " input-changed:", u+0, " non-finite:", v+0}' "$O/renders.txt" | tee -a "$O/renders.txt"
