#!/usr/bin/env bash
# Count how often each function is entered in a running process during SECS seconds (gdb, all loaded modules).
# Usage: count-hits.sh <process-name> <secs> <mangled-symbol>...   (on the VM gdb is the sudo wrapper)
P=$(pgrep -x "$1") || { echo "$1 is not running"; exit 1; }
SECS=$2; shift 2
CMDS=$(mktemp)
{ echo "set pagination off"; echo "set breakpoint pending on"
  for s in "$@"; do printf 'break %s\ncommands\nsilent\ncontinue\nend\n' "$s"; done
  echo "continue"; echo "info breakpoints"; echo "delete"; echo "detach"; } > "$CMDS"
timeout -s INT "$SECS" gdb -q -batch -p "$P" -x "$CMDS" 2>&1 | grep -E '^[0-9]+ +breakpoint|already hit' \
  | sed -E 's/^([0-9]+) +breakpoint.*/bp \1/'
rm -f "$CMDS"
echo "(bp N = the Nth symbol: $*)"
