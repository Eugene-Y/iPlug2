#!/usr/bin/env bash
# Attach gdb to a running process and print a backtrace of every thread if it crashes / exits.
# Runs INSIDE the GUI container, usually in the background:  gdb-catch.sh <pid> <outfile>
set -uo pipefail
P="$1"; OUT="$2"
gdb -q -batch -p "$P" </dev/null \
  -ex "set pagination off" -ex "handle SIGPIPE nostop noprint" -ex "handle SIG32 nostop noprint" -ex "handle SIG33 nostop noprint" \
  -ex continue -ex "info registers pc" -ex "bt 30" -ex "thread apply all bt 8" > "$OUT" 2>&1
