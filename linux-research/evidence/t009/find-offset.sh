#!/usr/bin/env bash
# T-009: where the Linux editor view (<view.png>) sits inside a mac screenshot (<mac.png>): a coarse subimage search
# at 1/4 size, then the best RMSE in a +-4 px window at full size. Prints "X Y RMSE". Runs where ImageMagick is.
set -u  # compare exits 1 when images differ, and its output has no trailing newline
MAC="$1"; VIEW="$2"; T=$(mktemp -d)
magick "$MAC" -resize 25% "$T/m.png"; magick "$VIEW" -resize 25% "$T/v.png"
read -r cx cy < <(magick compare -metric RMSE -subimage-search "$T/m.png" "$T/v.png" null: 2>&1 | sed -E 's/.*@ ([0-9]+),([0-9]+).*/\1 \2/')
read -r W H < <(magick identify -format '%w %h\n' "$VIEW")
best=""; for dx in $(seq -6 6); do for dy in $(seq -6 6); do
  x=$((cx*4+dx)); y=$((cy*4+dy)); [ $x -ge 0 ] && [ $y -ge 0 ] || continue
  magick "$MAC" -crop "${W}x${H}+$x+$y" +repage "$T/c.png"; r=$(magick compare -metric RMSE "$T/c.png" "$VIEW" null: 2>&1 | sed -E 's/^([0-9.e+]+).*/\1/') || true
  echo "$x $y $r"
done; done | sort -k3 -g | head -1
rm -rf "$T"
