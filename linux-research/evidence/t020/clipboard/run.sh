#!/usr/bin/env bash
# T-020: build and run clipboard-file-test in the GUI container (DISPLAY set by gx.sh). Usage: run.sh <iPlug2 tree>
set -u
R="$1"; HERE="$(cd "$(dirname "$0")" && pwd)"; B=/tmp/t020-clip; mkdir -p $B
g++ -std=c++17 -fsigned-char -I"$R/IPlug" -I"$R/IGraphics" -I"$R/IGraphics/Platforms" -I"$R/WDL" "$HERE/clipboard-file-test.cpp" -o $B/test -lX11 || exit 2
P='/tmp/a dir/Preset #1 ünï.fxp'
$B/test "$P" 4 & sleep 1
for t in TARGETS text/uri-list x-special/gnome-copied-files UTF8_STRING; do
  printf '== %s\n' "$t"; xclip -selection clipboard -t "$t" -o 2>&1 | od -c | head -4
done
wait
