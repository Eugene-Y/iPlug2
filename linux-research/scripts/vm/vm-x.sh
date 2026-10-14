#!/usr/bin/env bash
# Run a command on the VM inside the GNOME X11 session (DISPLAY, XAUTHORITY, session bus set), with the cwd at the
# mirrored linux-research/ and the harness's sudo gdb wrapper first on PATH. Exits with the command's status.
# Usage: vm-x.sh <cmd> [args...]        e.g. vm-x.sh xdotool search --name REAPER
#        VM_NOSYNC=1 skips copying linux-research/scripts to the VM first.
set -uo pipefail
. "$(dirname "$0")/vm-env.sh"
[ $# -gt 0 ] || { echo "usage: $0 <cmd> [args...]"; exit 2; }
[ "${VM_NOSYNC:-0}" = 1 ] || vm_sync_harness
# Daemons the command leaves behind (xclip serving a selection, jackd, REAPER) inherit its stdout; ssh would wait for
# them. The output goes through sed, which quits at the status line, so only sed holds the ssh channel.
vm "cd ~/$VM_HARNESS/linux-research && . scripts/vm/remote-bin/vm-display-env && { $(printf '%q ' "$@"); echo \"__VMX_RC=\$?\"; } 2>&1 | sed -u '/__VMX_RC=[0-9]*\$/q'" \
  | { rc=255; while IFS= read -r l; do case "$l" in
    *__VMX_RC=*) [ -n "${l%__VMX_RC=*}" ] && printf '%s\n' "${l%__VMX_RC=*}"; rc=${l##*__VMX_RC=};;
    *) printf '%s\n' "$l";;
  esac; done; exit "$rc"; }
