#!/usr/bin/env bash
# Thread census + all-thread backtraces of a running process. Runs INSIDE the GUI container.
# Usage: threads.sh <exact-process-name|pid> [bt-depth=16]
set -uo pipefail
T="$1"; DEPTH="${2:-16}"
if [[ "$T" =~ ^[0-9]+$ ]]; then P="$T"; else P=$(pgrep -n -x "$T"); fi
[ -n "$P" ] || { echo "process $T not found"; pgrep -a . | tail -20; exit 1; }
echo "pid=$P cmd=$(tr '\0' ' ' < /proc/$P/cmdline)"
echo "--- threads (count name)"; cat /proc/$P/task/*/comm | sort | uniq -c
echo "--- backtraces"
gdb -q -p "$P" -batch </dev/null -ex "set pagination off" -ex "thread apply all bt $DEPTH" 2>&1 \
  | grep -E '^Thread|^#' | sed -E 's/ \(\) from / from /' | cut -c1-220
