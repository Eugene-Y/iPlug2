#!/usr/bin/env bash
# sheet.sh <dir> <out.png> <names...>: labelled contact sheet of PNGs
d="$1"; o="$2"; shift 2; a=()
for n in "$@"; do a+=("$d/$n.png"); done
montage -label '%t' "${a[@]}" -tile 4x -geometry 640x400+4+4 -pointsize 18 "$o"
