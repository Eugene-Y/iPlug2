#!/usr/bin/env bash
# Screenshot the VM's desktop (or one X window) into a PNG on the Mac.
# Usage: vm-shot.sh <out.png> [window-id]
set -euo pipefail
. "$(dirname "$0")/vm-env.sh"
OUT="$1"; WIN="${2:-root}"
mkdir -p "$(dirname "$OUT")"
vm ". ~/$VM_HARNESS/linux-research/scripts/vm/remote-bin/vm-display-env 2>/dev/null || export DISPLAY=:0 XAUTHORITY=/run/user/\$(id -u)/gdm/Xauthority
    import -window $(printf '%q' "$WIN") png:-" > "$OUT"
echo "saved $OUT ($(wc -c < "$OUT" | tr -d ' ') bytes)"
