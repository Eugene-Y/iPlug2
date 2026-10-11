#!/usr/bin/env bash
# One-shot GUI test from the HOST (macOS): starts the GUI container if needed, then
#   X.clap / X.vst3  -> REAPER: open editor, run steps, close/reopen x5, 2nd instance, thread census
#   app binary       -> standalone under Xvfb+openbox (--no-io), run steps, thread census
# Usage: run-gui-test.sh <plugin-or-app> <outdir> [steps-file] [container=${GUI:-gui}]
# One container per person: the scripts use fixed /tmp paths and one X display inside it.
# Steps use the 1024x768 IPlugControls layout by default (scripts/steps/iplugcontrols.txt);
# pass /dev/null to only open/close.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
P="$1"; OUT="$2"; STEPS="${3:-scripts/steps/iplugcontrols.txt}"; C="${4:-${GUI:-gui}}"
cd "$HERE/.."
docker inspect -f '{{.State.Running}}' "$C" 2>/dev/null | grep -q true || "$HERE/gui-up.sh" "$C"
mkdir -p "$OUT"
NAME=$(basename "$P"); NAME="${NAME%.*}"
case "$P" in
  *.clap|*.vst3)
    FMT=$(echo "${P##*.}" | tr a-z A-Z)
    "$HERE/gx.sh" "$C" timeout 900 scripts/reaper-plugin-test.sh "$OUT" "$FMT: $NAME" "$P" "$STEPS" 2>&1 | tee "$OUT/run.log";;
  *)
    "$HERE/gx.sh" -d "$C" bash -c "'$P' --no-io > '$OUT/stderr.txt' 2>&1; echo EXIT=\$? >> '$OUT/stderr.txt'"
    sleep 5
    # plugin view = largest child of the app's top-level window
    ORIGIN=$("$HERE/gx.sh" "$C" bash -c "xwininfo -root -tree | awk '/\"$NAME\": \(\"$NAME\" \"$NAME\"\)/{f=1;next} f&&/^ +0x/{split(\$NF,a,\"+\"); print a[2],a[3]; exit}'")
    echo "view origin: $ORIGIN"
    (echo "origin $ORIGIN"; sed "s/__PROC__/$NAME/" "$STEPS") | "$HERE/gx.sh" "$C" env OUT="$OUT" scripts/xact.sh | tee "$OUT/xact.log"
    "$HERE/gx.sh" "$C" scripts/threads.sh "$NAME" 10 > "$OUT/threads.txt" 2>&1 || true
    "$HERE/gx.sh" "$C" pkill -x "$NAME" || true;;
esac
