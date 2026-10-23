#!/usr/bin/env bash
# For N seconds, count every call into functions matching a regex (gdb rbreak), per function and thread.
# Usage (GUI container): t038-allhits.sh <pid> <secs> <regex> [object file]
set -uo pipefail
P="$1"; SECS="$2"; RE="$3"; OBJ="${4:-}"
PY=$(mktemp /tmp/allh-XXXX.py)
cat > "$PY" <<PYEOF
import gdb, re
counts = {}
class B(gdb.Breakpoint):
    def stop(self):
        k = (self.location, gdb.selected_thread().ptid[1])
        counts[k] = counts.get(k, 0) + 1
        return False
def dump():
    for (loc, lwp), n in sorted(counts.items()):
        print("HITS %6d lwp=%d %s" % (n, lwp, loc))
gdb.events.exited.connect(lambda e: dump())
PYEOF
CMDS=$(mktemp /tmp/allh-XXXX)
{
  echo "set pagination off"; echo "set breakpoint pending on"; echo "source $PY"
  echo "python"
  echo "import gdb"
  echo "out = gdb.execute('info functions $RE', to_string=True)"
  echo "names = set()"
  echo "for line in out.splitlines():"
  echo "    line=line.strip()"
  echo "    m = __import__('re').match(r'^(?:\d+:\s*)?(?:static )?(?:.*?\s)?((?:iplug|Steinberg)::[\w:~]+)\(', line)"
  echo "    if m and 'thunk' not in line: names.add(m.group(1))"
  echo "for n in sorted(names):"
  echo "    try: B(n)"
  echo "    except Exception as e: pass"
  echo "print('breakpoints', len(names))"
  echo "end"
  echo "python import signal"
  echo "continue"
  echo "python dump()"
} > "$CMDS"
timeout -s INT "$SECS" gdb -q -batch -p "$P" -x "$CMDS" </dev/null 2>&1 | grep -E '^HITS|^breakpoints'
rm -f "$CMDS" "$PY"
