#!/usr/bin/env bash
# For N seconds, log every hit of the given member functions with the LWP of the calling thread and the
# object (`this`, the first argument register). Runs INSIDE the GUI container.
# Usage: gdb-hits.sh <pid> <seconds> <symbol> [symbol...]
# Output: one line per hit: "HIT <symbol> lwp=<tid> this=<hex>"; the main thread's lwp equals the pid.
set -uo pipefail
P="$1"; SECS="$2"; shift 2
case "$(uname -m)" in aarch64) REG='$x0';; x86_64) REG='$rdi';; *) echo "unsupported arch"; exit 2;; esac
CMDS=$(mktemp /tmp/gdbh-XXXX)
{
  echo "set pagination off"; echo "set breakpoint pending on"
  for s in "$@"; do
    echo "break $s"; echo "commands"; echo "silent"
    echo "python print('HIT %s lwp=%d this=0x%x' % ('$s', gdb.selected_thread().ptid[1], int(gdb.parse_and_eval('$REG')) & 0xffffffffffffffff))"
    echo "continue"; echo "end"
  done
  echo "continue"
} > "$CMDS"
timeout -s INT "$SECS" gdb -q -batch -p "$P" -x "$CMDS" </dev/null 2>&1 | grep -E '^HIT'
rm -f "$CMDS"
