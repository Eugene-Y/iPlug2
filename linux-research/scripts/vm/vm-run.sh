#!/usr/bin/env bash
# Run a Docker-harness script on the VM's real desktop and copy its <out> directory back.
# Usage: vm-run.sh <script> <out> [args...]      runs `scripts/<script> <out> [args...]` on the VM
#   <out>: relative to linux-research/ (e.g. evidence/vm-p3a/churn-controls-clap), emptied on the VM first
#   args starting with ~/ are expanded to the VM's $HOME (plug-in paths from vm-deploy.sh)
#   e.g. vm-run.sh editor-churn-test.sh evidence/vm-p3a/churn-clap "CLAP: IPlugControls" ~/.clap/IPlugControls.clap 50
# Env: VM_KEEP=1 leaves REAPER running afterwards (otherwise REAPER and jackd are stopped, desktop left clean);
#      VM_TIMEOUT (s, default 1200); VM_ENV="A=1 B=2" sets variables for the script on the VM.
set -uo pipefail
. "$(dirname "$0")/vm-env.sh"
SCRIPT="$1"; OUT="$2"; shift 2
vm_sync_harness
VMHOME=$(vm 'echo $HOME')
ARGS=(); for a in "$@"; do case "$a" in "~/"*) ARGS+=("$VMHOME/${a#\~/}");; *) ARGS+=("$a");; esac; done
mkdir -p "$(dirname "$LR/$OUT")"
vm "rm -rf ~/$VM_HARNESS/linux-research/$OUT; mkdir -p ~/$VM_HARNESS/linux-research/$OUT"
VM_NOSYNC=1 "$(dirname "$0")/vm-x.sh" bash -c vm_close_overview
VM_NOSYNC=1 "$(dirname "$0")/vm-x.sh" env ${VM_ENV:-} timeout "${VM_TIMEOUT:-1200}" "scripts/$SCRIPT" "$OUT" ${ARGS[@]+"${ARGS[@]}"} 2>&1 \
  | tee "$LR/$OUT.run.log.tmp"
rc=${PIPESTATUS[0]}
[ "${VM_KEEP:-0}" = 1 ] || vm "pkill -x reaper; pkill -x jackd; true"
vm_pull "$OUT"; mv "$LR/$OUT.run.log.tmp" "$LR/$OUT/run.log"
exit "$rc"
