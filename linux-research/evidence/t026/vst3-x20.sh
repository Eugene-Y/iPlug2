#!/usr/bin/env bash
# VST3 validator -e x20 on one build. Runs INSIDE the container. Usage: vst3-x20.sh <builddir> <outdir>
mkdir -p "$2"; n=0
for i in $(seq 1 20); do
  timeout 600 validator -e "$1/out/Sand.vst3" > "$2/run-$i.log" 2>&1 || n=$((n + 1))
  grep -E 'Result: [0-9]+ tests passed' "$2/run-$i.log" | tail -1
done | sort | uniq -c
echo "non-zero exits: $n"
