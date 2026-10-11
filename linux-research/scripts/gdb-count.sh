#!/usr/bin/env bash
# Count how often (and from which thread) functions are hit in a running process for N seconds.
# Runs INSIDE the GUI container. Usage: gdb-count.sh <proc-name|pid> <seconds> <symbol> [symbol...]
set -uo pipefail
T="$1"; SECS="$2"; shift 2
if [[ "$T" =~ ^[0-9]+$ ]]; then P="$T"; else P=$(pgrep -n -x "$T"); fi
CMDS=$(mktemp /tmp/gdbc-XXXX)
{
  echo "set pagination off"; echo "set breakpoint pending on"
  for s in "$@"; do
    echo "break $s"; echo "commands"; echo "silent"; echo "printf \"HIT %s tid=%d\\n\", \"$s\", \$_thread"; echo "continue"; echo "end"
  done
  echo "continue"
} > "$CMDS"
timeout -s INT "$SECS" gdb -q -batch -p "$P" -x "$CMDS" </dev/null 2>&1 | grep -E '^HIT' | sort | uniq -c
