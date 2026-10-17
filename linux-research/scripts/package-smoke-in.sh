#!/usr/bin/env bash
# Release-archive smoke test on a "customer machine". Runs INSIDE a runtime-only container (docker/runtime,
# started by package-smoke.sh) as root; the per-user steps run as the unprivileged user `customer`.
# Usage: package-smoke-in.sh <archive.tar.gz> <outdir>
# Steps: install.sh per user and --system, ldd, clap-validator + VST3 validator on the installed copies,
# REAPER (CLAP and VST3: open, reopen x5, 2nd instance, draw thread), the standalone, a .clap copied
# without its .resources folder, uninstall. RESULT line at the end; exit 1 on any FAIL.
set -uo pipefail
ARCHIVE="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; OUT="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
as_customer() { su customer -s /bin/bash -c "$*"; }

PKG="$(basename "$ARCHIVE" .tar.gz)"
NAME="${PKG%%_v*}"
echo "=== $PKG on $(. /etc/os-release && echo "$PRETTY_NAME") $(uname -m), glibc $(ldd --version | head -1 | awk '{print $NF}')"

as_customer "rm -rf ~/pkg && mkdir ~/pkg && tar -xzf '$ARCHIVE' -C ~/pkg"
SRC=/home/customer/pkg/$PKG
# The product's own data (presets) lives in ~/.local/share/<Name>; installs and uninstalls must leave it alone.
USER_DATA=/home/customer/.local/share/$NAME
as_customer "mkdir -p '$USER_DATA/Presets' && echo keep > '$USER_DATA/Presets/smoke-test.preset'"
as_customer "cd '$SRC' && ./install.sh" > "$OUT/install-user.txt" 2>&1 || fail "install.sh (user) exit $?"
as_customer "cd '$SRC' && ./install.sh" >> "$OUT/install-user.txt" 2>&1 || fail "install.sh (user, again) exit $?"
cat "$OUT/install-user.txt"
CLAP=/home/customer/.clap/$NAME.clap
VST3=/home/customer/.vst3/$NAME.vst3
APP=/home/customer/.local/lib/$NAME/$NAME
for f in "$CLAP" "$(dirname "$CLAP")/$NAME.resources" "$VST3" "$APP" /home/customer/.local/bin/$NAME \
         "/home/customer/.local/share/applications/$(echo "$NAME" | tr 'A-Z ' 'a-z-').desktop"; do
  [ -e "$f" ] || fail "per-user install: missing $f"
done

echo "--- ldd"
: > "$OUT/ldd.txt"
for f in "$CLAP" "$VST3"/Contents/*-linux/"$NAME".so "$APP"; do
  if ldd "$f" 2>&1 | grep 'not found' >> "$OUT/ldd.txt"; then fail "ldd $(basename "$f"): unresolved libraries"; fi
done
cat "$OUT/ldd.txt"

echo "--- validators (installed per-user copies, run as customer)"
as_customer "timeout 900 clap-validator validate '$CLAP'" 2>&1 | sed 's/\x1b\[[0-9;]*m//g' > "$OUT/clap-validator.txt"
echo "clap-validator exit=${PIPESTATUS[0]} :: $(grep -E 'tests run' "$OUT/clap-validator.txt" | tail -1)"
as_customer "timeout 900 validator -e '$VST3'" > /tmp/vst3-validator-full.txt 2>&1; rc=$?
# The full log is ~190 KB of passing test names; keep what explains a failure.
{ grep -iE 'fail|error' /tmp/vst3-validator-full.txt | grep -v ' 0 tests failed'; echo '...'; tail -25 /tmp/vst3-validator-full.txt; } > "$OUT/vst3-validator.txt"
echo "vst3 validator exit=$rc :: $(grep -E 'Result: [0-9]+ tests passed' "$OUT/vst3-validator.txt" | tail -1)"
grep -qE 'Result: [0-9]+ tests passed, 0 tests failed' "$OUT/vst3-validator.txt" || fail "VST3 validator (exit $rc)"
grep -qE '[0-9]+ tests run, [0-9]+ passed' "$OUT/clap-validator.txt" || fail "clap-validator did not run its tests"
# Known failures are the product's own, reported elsewhere; any other failing test fails the smoke test.
grep -oE "┌─ [a-z0-9-]+|FAILED" "$OUT/clap-validator.txt" | awk '/┌─/{n=$2} /FAILED/{print n}' | sort > "$OUT/clap-validator-failed.txt"
KNOWN="$HERE/known-clap-failures/$NAME.txt"
if [ -f "$KNOWN" ]; then
  new_fails=$(grep -vxF -f <(grep -v '^#' "$KNOWN") "$OUT/clap-validator-failed.txt" || true)
  [ -z "$new_fails" ] || fail "clap-validator: failures not in $(basename "$KNOWN"): $(echo $new_fails)"
fi
echo "clap-validator failed: $(echo $(cat "$OUT/clap-validator-failed.txt"))"

# gdb finds the split symbols by build id, as it would for a crash report from the field. Reading them takes
# a few seconds, hence the longer count window. Under emulation (SMOKE_EMULATED=1, x86_64 on Apple silicon)
# gdb breakpoints never fire and llvmpipe is slow: skip the count, wait longer for the first frame.
if [ "${SMOKE_EMULATED:-0}" = 1 ]; then
  export GDB_COUNT_SECS=0 REAPER_OPEN_WAIT=10
else
  export GDB_COUNT_SECS=20
fi
# A REAPER left behind by an earlier phase would take over the next start ("activating running instance").
stop_reaper() { pkill -x reaper 2>/dev/null; sleep 1; pkill -9 -x reaper 2>/dev/null; true; }
DEBUG_ARCHIVE="${ARCHIVE%.tar.gz}_debug-symbols.tar.gz"
if [ -f "$DEBUG_ARCHIVE" ]; then
  mkdir -p /usr/lib/debug && tar -xzf "$DEBUG_ARCHIVE" -C /usr/lib/debug --strip-components=1
fi

echo "--- REAPER"
for P in "$CLAP" "$VST3"; do
  FMT=$(echo "${P##*.}" | tr a-z A-Z)
  stop_reaper
  timeout -k 10 900 "$HERE/reaper-plugin-test.sh" "$OUT/reaper-${FMT,,}" "$FMT: $NAME" "$P" "" > "$OUT/reaper-${FMT,,}.log" 2>&1
  echo "$FMT: $(tail -n +1 "$OUT/reaper-${FMT,,}/RESULT.txt" 2>/dev/null | head -1)"
  grep -q 'RESULT: PASS' "$OUT/reaper-${FMT,,}/RESULT.txt" 2>/dev/null || fail "REAPER $FMT (see reaper-${FMT,,}.log)"
done

echo "--- standalone (as customer, --no-io)"
stop_reaper
as_customer "DISPLAY=:99 '$APP' --no-io > /tmp/app-stderr.txt 2>&1 & echo \$! > /tmp/app.pid"
sleep 6
cp /tmp/app-stderr.txt "$OUT/app-stderr.txt" 2>/dev/null
if xdotool search --onlyvisible --name "^$NAME\$" >/dev/null 2>&1 && kill -0 "$(cat /tmp/app.pid)" 2>/dev/null; then
  import -window root "$OUT/app.png"; echo "standalone window up"
else
  fail "standalone: no window or process died"; import -window root "$OUT/app.png"
fi
# By PID: under Rosetta neither the name nor the command line matches the binary.
kill "$(cat /tmp/app.pid)" 2>/dev/null; sleep 1; kill -9 "$(cat /tmp/app.pid)" 2>/dev/null

echo "--- CLAP without its .resources folder"
mkdir -p /tmp/nores && cp "$CLAP" /tmp/nores/
stop_reaper
timeout -k 10 600 "$HERE/reaper-plugin-test.sh" "$OUT/nores" "CLAP: $NAME" /tmp/nores/"$NAME".clap "" > "$OUT/nores.log" 2>&1
grep -E 'REAPER ALIVE|render\[|RESULT' "$OUT/nores.log" | head -8
grep -i 'iPlug2:' /tmp/reaper.log > "$OUT/nores-messages.txt"
cat "$OUT/nores-messages.txt"
grep -q "resource folder .*/$NAME.resources/ not found" "$OUT/nores-messages.txt" \
  || fail "CLAP without resources: no message naming the missing folder"
grep -q 'REAPER pid .* dead' "$OUT/nores.log" && fail "CLAP without resources crashed REAPER"
stop_reaper

echo "--- system install"
(cd "$SRC" && ./install.sh --system) > "$OUT/install-system.txt" 2>&1 || fail "install.sh --system exit $?"
cat "$OUT/install-system.txt"
for f in /usr/lib/clap/$NAME.clap /usr/lib/clap/$NAME.resources /usr/lib/vst3/$NAME.vst3 /opt/$NAME/$NAME /usr/local/bin/$NAME; do
  [ -e "$f" ] || fail "system install: missing $f"
done
unreadable=$(find /usr/lib/clap/$NAME.* /usr/lib/vst3/$NAME.vst3 /opt/$NAME ! -perm -o+r | head -5)
[ -z "$unreadable" ] || fail "system install: not world-readable: $unreadable"
as_customer "timeout 300 validator '/usr/lib/vst3/$NAME.vst3'" > "$OUT/vst3-validator-system.txt" 2>&1 \
  || fail "VST3 validator on the system copy (as customer)"
as_customer "timeout 300 clap-validator validate --only-failed -t 'descriptor|features' '/usr/lib/clap/$NAME.clap'" \
  > "$OUT/clap-validator-system.txt" 2>&1 || fail "clap-validator on the system copy (as customer)"

echo "--- uninstall"
(cd "$SRC" && ./install.sh --system --uninstall) >/dev/null 2>&1
as_customer "cd '$SRC' && ./install.sh --uninstall" >/dev/null 2>&1
left=$(ls -d /usr/lib/clap/$NAME.* /usr/lib/vst3/$NAME.vst3 /opt/$NAME /usr/local/bin/$NAME \
           /home/customer/.clap/$NAME.* /home/customer/.vst3/$NAME.vst3 /home/customer/.local/lib/$NAME \
           /home/customer/.local/bin/$NAME 2>/dev/null)
[ -z "$left" ] || fail "uninstall left: $left"
[ -f "$USER_DATA/Presets/smoke-test.preset" ] || fail "install/uninstall removed the user's data in $USER_DATA"

if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
[ ${#FAILS[@]} -eq 0 ]
