#!/usr/bin/env bash
# Screenshot the whole Xvfb screen (or one window id) to PNG. Runs INSIDE the GUI container.
# Usage: shot.sh <out.png> [window-id]
set -euo pipefail
OUT="$1"; WIN="${2:-root}"
mkdir -p "$(dirname "$OUT")"
import -display "${DISPLAY:-:99}" -window "$WIN" "$OUT"
echo "saved $OUT"
