#!/usr/bin/env bash
# T-013: run carla-probe.py on the VM's desktop; plug-in stderr (host-loop and drop traces) to <out>/stderr.txt.
# Usage (through vm-run.sh): carla-run.sh <out> <carla-src-dir> <clap|vst3> <plugin-path> [clap-id]
OUT="$1"; shift
mkdir -p "$OUT"
pgrep -x jackd >/dev/null || { jackd --no-realtime -d dummy -r 48000 -p 256 >/tmp/jackd.log 2>&1 & sleep 1.5; }
timeout 120 python3 "$(dirname "$0")/carla-probe.py" "$1" "$2" "$3" "${4:-}" "$OUT" 2> "$OUT/stderr.txt" | tee "$OUT/steps.txt"
rc=${PIPESTATUS[0]}
grep -c 'OFF-MAIN' "$OUT/stderr.txt" | sed 's/^/off-main trace lines: /'
grep -o 'backend=[a-z0-9()-]*' "$OUT/stderr.txt" | sort | uniq -c
exit "$rc"
