#!/usr/bin/env bash
# T-025: run the VST3 validator -e N times on one bundle; count runs that crash, report an ASan error, or end
# without the validator's summary. An ASan build is detected and libasan preloaded (the validator is not ASan).
# Runs INSIDE the container (dock.sh). Usage: t025-validator-loop.sh <bundle.vst3> <outdir> [N=20]
set -uo pipefail
B="$1"; OUT="$2"; N="${3:-20}"
mkdir -p "$OUT"
PRELOAD=""
if ldd "$(find "$B" -name '*.so' | head -1)" | grep -q libasan; then
  PRELOAD="$(gcc -print-file-name=libasan.so)"
  export ASAN_OPTIONS=detect_leaks=0
fi
failed=0; asan=0; race=0; incomplete=0
for i in $(seq "$N"); do
  LD_PRELOAD="$PRELOAD" timeout 300 validator -e "$B" > "$OUT/run-$i.txt" 2>&1; rc=$?
  bad=0
  grep -q 'ERROR: AddressSanitizer' "$OUT/run-$i.txt" && { asan=$((asan+1)); bad=1; }
  grep -q 'NChanDelayLine' "$OUT/run-$i.txt" && race=$((race+1))
  grep -q 'Result:' "$OUT/run-$i.txt" || { incomplete=$((incomplete+1)); bad=1; }
  [ $rc -ge 128 ] && bad=1
  failed=$((failed+bad))
done
echo "runs=$N failed=$failed asan=$asan delay-line-in-stack=$race incomplete=$incomplete preload=${PRELOAD:+libasan}" | tee "$OUT/summary.txt"
