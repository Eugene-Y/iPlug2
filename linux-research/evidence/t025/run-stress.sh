#!/usr/bin/env bash
# T-025: run each stress binary N times; a run fails on a sanitizer report, a crash, or a bad block.
# Runs INSIDE the container. Usage: run-stress.sh <dir with stress-address, stress-thread> [N=10] [seconds=3] [setterPauseUs=0]
set -uo pipefail
D="$1"; N="${2:-10}"; S="${3:-3}"; P="${4:-0}"
for san in address thread; do
  fail=0
  for i in $(seq "$N"); do
    timeout 60 setarch "$(uname -m)" -R "$D/stress-$san" "$S" 4096 "$P" > "$D/run-$san-$i.txt" 2>&1 || fail=$((fail+1))
  done
  first=$(grep -l -m1 'ERROR: AddressSanitizer\|WARNING: ThreadSanitizer\|bad-blocks=[1-9]' "$D"/run-$san-*.txt | head -1)
  echo "$san: runs=$N failed=$fail ${first:+first=$(basename "$first")}"
done | tee "$D/summary.txt"
