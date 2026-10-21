# Log which threads call the given functions in a running Renoise: attaches gdb (the sudo wrapper) for SECS and
# logs every hit with the caller's LWP (the main thread's LWP is the pid), while it runs the actions a few seconds
# apart: "lua:<chunk>" goes to Renoise through rn-lua.py, "sh:<command>" runs in this shell (xdotool, ...).
# Usage: call-trace.sh <out-file> <secs> <symbols-file> <action> [action ...]
# Symbols are linkage (mangled) names, one per line; '#' lines are comments; "<symbol> bt" also logs a backtrace,
# "<symbol> x <gdb expression>" also logs the expression's value as a long (e.g. *(long*)($x1+64)).
# arm64 only: every hit logs the argument registers $x0/$x1, and the .syms offsets assume that ABI.
OUT=$1; SECS=$2; SYMS=$3; shift 3
HERE=$(cd "$(dirname "$0")" && pwd)
P=$(pgrep -x renoise) || { echo "renoise is not running"; exit 1; }
CMDS=$(mktemp /tmp/rntrace-XXXX)
{
  echo "set pagination off"; echo "set breakpoint pending on"
  printf '%s\n' python 'def t013_bt(depth=10):' '  f, names = gdb.newest_frame(), []' \
    '  while f is not None and len(names) < depth:' '    names.append(f.name() or "?"); f = f.older()' \
    '  print("BT " + " <- ".join(names))' end
  grep -v -E '^\s*(#|$)' "$SYMS" | while read -r s opt expr; do
    echo "break $s"; echo "commands"; echo "silent"
    [ "$opt" = bt ] && echo "python t013_bt()"
    [ "$opt" = x ] && echo "printf \"  X $expr = %ld\\n\", $expr"
    echo "python print('HIT %s lwp=%d x0=0x%x x1=%d' % ('$s', gdb.selected_thread().ptid[1], int(gdb.parse_and_eval('\$x0')) & 0xffffffffffffffff, int(gdb.parse_and_eval('\$x1')) & 0xffffffff))"
    echo "continue"; echo "end"
  done
  echo "continue"
  # reached when the INT below stops the process: disarm, then detach (a detach with armed breakpoints can SIGTRAP it)
  echo "delete"; echo "detach"
} > "$CMDS"
echo "pid=$P (main LWP)" > "$OUT"
( timeout -s INT "$SECS" gdb -q -batch -p "$P" -x "$CMDS" </dev/null 2>&1 | grep -E '^HIT|^BT|^  X |warning: (Error|Cannot)|^Function .* not defined' >> "$OUT" ) &
G=$!
sleep 3
for a in "$@"; do
  echo "ACTION $(date +%T.%N | cut -c1-12) $a" >> "$OUT"
  case "$a" in
    lua:*) python3 "$HERE/rn-lua.py" "${a#lua:}" ;;
    sh:*)  bash -c "${a#sh:}" ;;
  esac
  sleep ${STEP:-3}
done
wait $G
rm -f "$CMDS"
cat "$OUT"
