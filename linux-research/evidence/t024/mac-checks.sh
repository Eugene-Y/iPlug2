#!/usr/bin/env bash
# mac counterpart of run-checks.sh (no ASan: it hangs on this Mac).
# Usage: mac-checks.sh <Sand.clap> <Sand.vst3 or -> <outdir> <render-clap> <clap-validator> <vst3-validator>
set -uo pipefail
CLAP="$1"; VST3="$2"; O="$3"; RENDER="$4"; CV="$5"; VV="$6"
mkdir -p "$O/render"

"$CV" validate "$CLAP" > "$O/clap-validator.txt" 2>&1
echo "clap-validator: $(grep -E 'tests run' "$O/clap-validator.txt" | tail -1)"

if [ "$VST3" != "-" ]; then
  crashes=0; : > "$O/vst3-x20.txt"
  for i in $(seq 1 20); do
    "$VV" -e "$VST3" > "$O/vst3-run-$i.txt" 2>&1; rc=$?
    [ $rc -ne 0 ] && crashes=$((crashes + 1))
    echo "run $i exit=$rc $(grep -E 'Result: [0-9]+ tests passed' "$O/vst3-run-$i.txt" | tail -1)" >> "$O/vst3-x20.txt"
  done
  echo "vst3 validator -e x20: $crashes non-zero exits" | tee -a "$O/vst3-x20.txt"
fi

render() { local name="$1"; shift; "$RENDER" "$CLAP" "$O/render/$name.raw" "$@" 2>/dev/null | sed "s/^/$name: /"; }
{
  render default-48k-512-f32
  render default-48k-512-f64 f64=1
  render default-48k-512-f64-inplace f64=1 inplace=1
  render default-44k-64-f32 sr=44100 block=64
  render default-96k-1000-f64 sr=96000 block=1000 f64=1
  render random-blocks-48k-f32 block=0 seed=7
  render order3-os2-48k-f32 p3=3 p15=1
  render grind-am-48k-f64 f64=1 p18=0.7 p22=0.5 p23=0.5
  for sr in 1000 22050 192000 384000 768000; do render "default-${sr}" sr=$sr frames=$sr; done
} | tee "$O/render.txt"
( cd "$O/render" && shasum -a 256 *.raw > ../render.sha256 )
