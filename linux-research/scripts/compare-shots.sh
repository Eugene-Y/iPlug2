#!/usr/bin/env bash
# Side-by-side and zoomed crops of a Linux and a macOS screenshot of the same editor (runs INSIDE a container).
# Usage: compare-shots.sh <outdir> <linux.png> <mac.png> <W> <H>
set -euo pipefail
O="$1"; L="$2"; M="$3"; W="$4"; H="$5"
convert "$M" -resize "${W}x${H}!" "$O/mac-1x.png"
convert "$L" "$O/mac-1x.png" +append "$O/side-by-side.png"
convert \( "$L" -crop 320x200+0+0 +repage -scale 200% \) \( "$O/mac-1x.png" -crop 320x200+0+0 +repage -scale 200% \) +append "$O/zoom-topleft.png"
convert \( "$L" -crop 320x200+130+280 +repage -scale 200% \) \( "$O/mac-1x.png" -crop 320x200+130+280 +repage -scale 200% \) +append "$O/zoom-knobs.png"
compare -metric PAE "$L" "$O/mac-1x.png" "$O/diff.png" 2>&1 || true
