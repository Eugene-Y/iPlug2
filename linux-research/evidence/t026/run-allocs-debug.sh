#!/usr/bin/env bash
# N1 audit: allocations inside process() in a logging-enabled (RelWithDebInfo) build. Runs INSIDE the container.
# Usage: run-allocs-debug.sh <builddir> <render-t026>
C="$1/out/Sand.clap"; R="$2"
E="$(cd "$(dirname "$0")" && pwd)"
A="p3=3 p14=0.5 p16=0.3 p17=1.6 p18=0.7 p21=2 p22=0.5 p23=0.5"
for args in "frames=96000" "frames=96000 $A sweep=5:0:1" "frames=96000 $A blocks=512,64 zeroevery=3"; do
  echo "== $args"
  "$R" "$C" - $args 2>/dev/null | grep -o "allocs=[0-9]* alloc-bytes=[0-9]* allocs-first-block=[0-9]* alloc-blocks-after-first=[0-9]*"
done
echo "== attribution, joint mix swept (first 6 allocations):"
"$R" "$C" - frames=4096 bt=6 $A sweep=5:0:1 2>&1 >/dev/null | "$E/symbolize.sh" \
  | grep -oE "Sand-t026-linux/[^ ]+|host_info_model.hpp:[0-9]+|plog/[A-Za-z/]+\.h:[0-9]+" | sort | uniq -c
