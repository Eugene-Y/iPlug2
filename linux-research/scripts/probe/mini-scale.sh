#!/usr/bin/env bash
# T-007: scale scenarios of mini-clap-host-scale.cpp against one .clap, plus a screenshot of the editor per Xft.dpi.
# Runs INSIDE a GUI container. Usage: mini-scale.sh <host-binary> <plugin.clap> <outdir>
set -uo pipefail
H="$1"; P="$2"; OUT="$3"; HERE="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$OUT"
sizes() { grep -E 'editor|get_size|request|set_scale'; }
shot() { # <png>: the mini host's editor window, 1:1
  "$H" "$P" scale 4 >/dev/null 2>&1 &
  sleep 2.5
  local w; w=$(xdotool search --name mini-clap-host | head -1)
  read -r X Y W HH < <(xwininfo -id "$w" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} /Width:/{w=$NF} /Height:/{h=$NF} END{print x,y,w,h}')
  import -window root -crop "${W}x${HH}+$X+$Y" +repage "$1"
  wait
}
{
  for dpi in - 120 144 192; do
    "$HERE/xft-dpi.sh" "$dpi" >/dev/null 2>&1
    echo "== Xft.dpi $dpi"; "$H" "$P" scale 1 | sizes
    shot "$OUT/xft-${dpi/-/none}.png"
  done
  "$HERE/xft-dpi.sh" 144 >/dev/null 2>&1
  echo "== Xft.dpi 144, set_scale 2 by the host"; MINI_SET_SCALE=2 "$H" "$P" scale 1 | sizes
  echo "== Xft.dpi 144: rescale to 2 while open"; "$H" "$P" rescale 2 | sizes
  echo "== Xft.dpi 144: rescale to 1 while open"; "$H" "$P" rescale 1 | sizes
  echo "== Xft.dpi 144: reopen"; "$H" "$P" reopen | sizes
  "$HERE/xft-dpi.sh" - >/dev/null 2>&1
  echo "== GDK_SCALE=2, no Xft.dpi"; GDK_SCALE=2 "$H" "$P" scale 1 | sizes
  echo "== QT_SCALE_FACTOR=1.5, no Xft.dpi"; QT_SCALE_FACTOR=1.5 "$H" "$P" scale 1 | sizes
} > "$OUT/scale.txt" 2>&1
cat "$OUT/scale.txt"
