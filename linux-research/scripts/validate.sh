#!/usr/bin/env bash
# Run clap-validator / VST3 validator on built plugins. Runs INSIDE the container.
# Usage: validate.sh <outdir> <X.clap|X.vst3> ...
# Validators load the plugin without a display; GUI-touching tests are excluded by the tools.
set -uo pipefail
OUT="$1"; shift; mkdir -p "$OUT"
for P in "$@"; do
  n=$(basename "$P")
  case "$P" in
    *.clap)
      timeout 600 clap-validator validate --only-failed "$P" > "$OUT/clap-validator-$n.txt" 2>&1
      echo "clap-validator $n exit=$? :: $(grep -E 'tests run' "$OUT/clap-validator-$n.txt" | tail -1)"
      timeout 600 clap-validator validate "$P" > "$OUT/clap-validator-$n.full.txt" 2>&1 ;;
    *.vst3)
      timeout 600 validator -e "$P" > "$OUT/vst3-validator-$n.txt" 2>&1
      rc=$?
      echo "vst3 validator $n exit=$rc :: $(grep -E 'Result: [0-9]+ tests passed, [0-9]+ tests failed' "$OUT/vst3-validator-$n.txt" | tail -1)" ;;
  esac
done
