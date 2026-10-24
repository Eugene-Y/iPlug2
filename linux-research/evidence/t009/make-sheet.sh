#!/usr/bin/env bash
# T-009: side-by-side sheet: mac reference (cropped to the editor at X,Y) | Linux view | difference (red = differs).
# Usage: make-sheet.sh <mac.png> <linux-view.png> <X> <Y> <out.png> [H override, for taller mac views]
set -u
MAC="$1"; VIEW="$2"; X="$3"; Y="$4"; OUT="$5"
read -r W H < <(magick identify -format '%w %h\n' "$VIEW")
[ -n "${6:-}" ] && H="$6"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
magick "$MAC" -crop "${W}x${H}+$X+$Y" +repage "$T/mac.png"
magick "$VIEW" -crop "${W}x${H}+0+0" +repage "$T/lin.png"
magick compare -fuzz 8% "$T/mac.png" "$T/lin.png" -highlight-color red -lowlight-color none -compose src "$T/diff.png" 2>/dev/null
magick "$T/lin.png" "$T/diff.png" -composite "$T/diffover.png"
rmse=$(magick compare -metric RMSE "$T/mac.png" "$T/lin.png" null: 2>&1 | sed -E 's/^[0-9.e+]+ \(([0-9.e+-]+)\).*/\1/')
magick montage -label "${MAC_LABEL:-mac (Retina)}" "$T/mac.png" -label "${LIN_LABEL:-Linux (REAPER, scale 2)}" "$T/lin.png" -label "differs >8%: red (RMSE $rmse)" "$T/diffover.png" \
  -tile 1x3 -geometry +0+12 -pointsize 22 -background '#202020' -fill white "$OUT"
echo "$OUT rmse=$rmse"
