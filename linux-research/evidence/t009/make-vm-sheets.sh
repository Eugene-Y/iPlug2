#!/usr/bin/env bash
# T-009, VM part: for every view, (1) the mac reference shrunk to 50% | the VM capture at scale 1 | difference, and
# (2) the Docker capture | the VM capture | difference, both at scale 1. Prints a table (also in vm-x1-results.txt).
# Usage: make-vm-sheets.sh   (from evidence/t009; needs ImageMagick 7)
set -u
cd "$(dirname "$0")"
mkdir -p sheets mac-refs-half
views=(
  "gneiss g01-clean-moog-all|gneiss  clean moog all.png"
  "gneiss g02-freqs|gneiss freq tab.png"
  "gneiss g03-user-morph-off|gneiss morph off user tab.png"
  "gneiss g04-user-morph-on-tall|gneiss default state morph on user tab.png"
  "gneiss g05-about|gneiss about box.png"
  "gneiss g06-factory-presets|gneiss factory presets (mouse not captured).png"
  "gneiss g07-midi-cc-menu|gneiss main freq knob midi cc menu.png"
  "sand s01-flt|sand flt tab.png"
  "sand s02-env|sand env tab.png"
  "sand s03-grind|sand  grind tab.png"
  "sand s04-pan|sand pan tab.png"
  "sand s05-separate-dry-wet|sand  separate dry wet.png"
  "sand s06-debug-bounds|sand debug show control bounds on.png"
  "sand s07-input|sand input.png"
  "sand s08-about|sand about.png"
)
printf '%-28s %-14s %-14s %s\n' view mac50-vs-vm docker-vs-vm "pixels >8% (docker vs vm)" | tee vm-x1-results.txt
for v in "${views[@]}"; do
  prod=${v%% *}; rest=${v#* }; name=${rest%%|*}; ref=${rest#*|}
  vm="vm-x1-$prod/$name-view.png"; dk="docker-x1-$prod/$name-view.png"
  [ -f "mac-refs-half/$ref" ] || magick "mac-refs/$ref" -resize 50% "mac-refs-half/$ref"
  read -r x y _ < <(./find-offset.sh "mac-refs-half/$ref" "$vm")
  m=$(MAC_LABEL="mac (Retina, shrunk 50%)" LIN_LABEL="Linux VM (GNOME X11, virgl), scale 1" ./make-sheet.sh "mac-refs-half/$ref" "$vm" "$x" "$y" "sheets/vm-x1-$name.png" | sed 's/.*rmse=//')
  d=$(MAC_LABEL="Linux Docker (llvmpipe), scale 1" LIN_LABEL="Linux VM (virgl), scale 1" ./make-sheet.sh "$dk" "$vm" 0 0 "sheets/vm-vs-docker-x1-$name.png" | sed 's/.*rmse=//')
  ae=$(magick compare -metric AE -fuzz 8% "$dk" "$vm" null: 2>&1)
  printf '%-28s %-14s %-14s %s\n' "$name" "$m" "$d" "$ae" | tee -a vm-x1-results.txt
done
