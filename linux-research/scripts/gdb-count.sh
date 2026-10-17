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
  # Reached when the INT below stops the process: remove the breakpoints while it is stopped, then let go.
  # Detaching with them still armed races a thread that has just hit one (REAPER then dies of SIGTRAP).
  echo "delete"; echo "detach"
} > "$CMDS"
# Stop gdb with SIGINT so it detaches and removes its breakpoints. With breakpoints firing hundreds of times a
# second it can miss one INT, so repeat it; SIGKILL is the last resort, since a killed gdb leaves its breakpoint
# traps in the process, which then dies on the next hit.
OUTF=$(mktemp /tmp/gdbo-XXXX)
gdb -q -batch -p "$P" -x "$CMDS" </dev/null > "$OUTF" 2>&1 &
G=$!
sleep "$SECS"
for i in $(seq 30); do kill -0 $G 2>/dev/null || break; kill -INT $G 2>/dev/null; sleep 1; done
kill -0 $G 2>/dev/null && { echo "gdb-count: gdb ignored SIGINT for 30 s, killing it" >&2; kill -9 $G; }
wait $G 2>/dev/null
grep -E '^HIT .* tid=[0-9]+$' "$OUTF" | sort | uniq -c
rm -f "$OUTF" "$CMDS"
