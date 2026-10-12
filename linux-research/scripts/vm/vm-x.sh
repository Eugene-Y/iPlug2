#!/usr/bin/env bash
# Run a command on the VM inside the GNOME X11 session (DISPLAY, XAUTHORITY, session bus set), with the cwd at the
# mirrored linux-research/ and the harness's sudo gdb wrapper first on PATH.
# Usage: vm-x.sh <cmd> [args...]        e.g. vm-x.sh xdotool search --name REAPER
#        VM_NOSYNC=1 skips copying linux-research/scripts to the VM first.
set -euo pipefail
. "$(dirname "$0")/vm-env.sh"
[ $# -gt 0 ] || { echo "usage: $0 <cmd> [args...]"; exit 2; }
[ "${VM_NOSYNC:-0}" = 1 ] || vm_sync_harness
vm "cd ~/$VM_HARNESS/linux-research && . scripts/vm/remote-bin/vm-display-env && $(printf '%q ' "$@")"
