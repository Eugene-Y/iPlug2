#!/usr/bin/env bash
# T-007: GNOME fractional scaling on X11 (Mutter's experimental x11-randr-fractional-scaling, as Ubuntu's Settings
# offers it) on the VM: applies <scale> on <mode> through Mutter's DisplayConfig, runs scale-desktop.sh at it, and
# restores the mode, scale 1 and the experimental features on exit.
# Usage (host side): vm-run.sh vm/probe/scale-fractional.sh <out> <scale> <mode id> <REAPER's scale> <the APP's scale> [what...]
# ⚠ On this VM (Mesa virgl) gnome-shell hung after the fractional modeset (2026-10-14): its main thread waits in Mesa's
#   DRI3 buffer-age query, D-Bus calls to the shell and Mutter time out, the root window reads black. Any xrandr request
#   (`xrandr --output Virtual-1 --auto`) wakes it; restore() ends with one. UTM's display agent may then leave the
#   screen at another size than before (1920x863 instead of 1440x863 that day).
set -uo pipefail
OUT="$1"; SCALE="$2"; MODE="$3"; RS="$4"; AS="$5"; shift 5
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
OLD_FEATURES=$(gsettings get org.gnome.mutter experimental-features)
OLD_MODE=$(python3 "$HERE/vm/probe/mutter-scale.py" --state | sed -n "s/^modes: \[('[^']*', '\([^']*\)'.*/\1/p")
restore() {
  timeout 40 python3 "$HERE/vm/probe/mutter-scale.py" 1 "$OLD_MODE"
  gsettings set org.gnome.mutter experimental-features "$OLD_FEATURES"; sleep 2
  timeout 10 xrandr --output "$(xrandr | awk '/ connected/{print $1; exit}')" --auto; sleep 2
  python3 "$HERE/vm/probe/mutter-scale.py" --state | sed 's/^/restored /'
}
trap restore EXIT
python3 "$HERE/vm/probe/mutter-scale.py" "$SCALE" "$MODE" | tee "$OUT/mutter.txt"
python3 "$HERE/vm/probe/mutter-scale.py" --state | tee -a "$OUT/mutter.txt"
xrandr --verbose | grep -E 'connected|Transform|current' -A0 | head -6 | tee -a "$OUT/mutter.txt"
"$HERE/vm/probe/scale-desktop.sh" "$OUT" - 0 1.0 "$RS" "$AS" "$@"
