#!/usr/bin/env bash
# sheet2.sh <out.png> <file.png>...: labelled contact sheet (label = parent/name)
o="$1"; shift; a=()
for f in "$@"; do a+=(-label "$(basename "$(dirname "$f")" | sed 's/reaper-//')/$(basename "$f" .png)" "$f"); done
montage "${a[@]}" -tile 3x -geometry 640x400+4+4 -pointsize 16 "$o"
