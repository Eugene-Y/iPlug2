#!/usr/bin/env bash
# Contact sheet of a run's screenshots, cropped to the editor view (runs INSIDE a container).
# Usage: sheet.sh <dir> <X> <Y> [W=1024] [H=768] [glob=c*.png] [out=sheet.png]
set -euo pipefail
D="$1"; X="$2"; Y="$3"; W="${4:-1024}"; H="${5:-768}"; G="${6:-c*.png}"; O="${7:-sheet.png}"
cd "$D"
montage -label '%t' $G -crop "${W}x${H}+${X}+${Y}" +repage -tile 4x -geometry 512x384+4+4 "$O"
