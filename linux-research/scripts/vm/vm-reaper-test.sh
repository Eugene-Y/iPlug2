#!/usr/bin/env bash
# reaper-plugin-test.sh on the VM's real GNOME desktop: open, steps, reopen x5, 2nd instance, main-thread check.
# Results land in <out> on the VM and are copied back into the same path here.
# Usage: vm-reaper-test.sh <out> "<FMT: Name>" <plugin on the VM> [steps]
#   <out>, [steps]: relative to linux-research/ (e.g. evidence/vm-p3a/reaper-controls-clap, scripts/steps/iplugcontrols-p3a.txt)
#   <plugin>: e.g. ~/.clap/IPlugControls.clap, ~/.vst3/IPlugControls.vst3 (vm-deploy.sh puts them there)
# Env: VM_KEEP=1 leaves REAPER running afterwards (otherwise REAPER and jackd are stopped, desktop left clean).
set -uo pipefail
. "$(dirname "$0")/vm-env.sh"
OUT="$1"; FX="$2"; PLUG="$3"; STEPS="${4:-}"
vm_sync_harness
case "$PLUG" in "~/"*) PLUG="$(vm 'echo $HOME')/${PLUG#\~/}";; esac
vm "rm -rf ~/$VM_HARNESS/linux-research/$OUT"
VM_NOSYNC=1 "$(dirname "$0")/vm-x.sh" timeout 900 scripts/reaper-plugin-test.sh "$OUT" "$FX" "$PLUG" $STEPS 2>&1 | tee "$LR/.vm-run.log"
rc=${PIPESTATUS[0]}
[ "${VM_KEEP:-0}" = 1 ] || vm "pkill -x reaper; pkill -x jackd; true"
vm_pull "$OUT"; mv "$LR/.vm-run.log" "$LR/$OUT/run.log"
exit "$rc"
