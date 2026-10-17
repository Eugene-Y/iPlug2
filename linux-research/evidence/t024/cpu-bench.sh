#!/usr/bin/env bash
# process() time of Sand, best of N runs, for two builds run alternately (same machine state for both).
# Usage: cpu-bench.sh <render-clap> <before.clap> <after.clap> [runs=7] [render args...]
set -uo pipefail
R="$1"; B="$2"; A="$3"; N="${4:-7}"; shift 4 2>/dev/null || shift $#
ARGS=("$@"); [ ${#ARGS[@]} -eq 0 ] && ARGS=(frames=4800000 f64=1)
best() { sort -g | head -1; }
tb=(); ta=()
for i in $(seq 1 "$N"); do
  tb+=("$("$R" "$B" - "${ARGS[@]}" 2>/dev/null | sed -n 's/.*process-s=\([0-9.]*\).*/\1/p')")
  ta+=("$("$R" "$A" - "${ARGS[@]}" 2>/dev/null | sed -n 's/.*process-s=\([0-9.]*\).*/\1/p')")
done
echo "args: ${ARGS[*]}  runs: $N"
echo "before: best $(printf '%s\n' "${tb[@]}" | best) s  all: ${tb[*]}"
echo "after:  best $(printf '%s\n' "${ta[@]}" | best) s  all: ${ta[*]}"
