#!/usr/bin/env bash
# Case 5 in Qtractor (VST3): with Gneiss' editor closed and the transport playing, set "oversampling" in Qtractor's
# plug-in form (open at Page 2) to 1x, read the track's plug-in latency in Track > Plugins, then 8x and read it again,
# all under call-trace.sh with latency.syms. Coordinates are those of t013vst3.qtr on the 1368x863 VM screen.
# Usage (vm-x.sh, Qtractor running, the form open): latency-closed-seq.sh <out-dir>
OUT=$1; mkdir -p "$OUT"; H=$(cd "$(dirname "$0")" && pwd)
c='c(){ xdotool mousemove $1 $2 mousedown 1; sleep 0.08; xdotool mouseup 1; sleep ${3:-1}; }'
os="xdotool mousemove 566 376 click 1 key ctrl+a type"
track="xdotool mousemove 290 240 click --repeat 2 --delay 80 1; sleep 2; $c; c 573 331 1"
form="xdotool mousemove 395 259 mousedown 3; sleep 0.1; xdotool mouseup 3; sleep 1; $c; c 456 556 2"
STEP=4 PROC=qtractor bash "$H/../renoise/call-trace.sh" "$OUT/latency-editor-closed.txt" 75 "$H/latency.syms" \
  "sh:$os 0; xdotool key Return" "sh:$c; c 1007 227" "sh:$track" "sh:import -window root $OUT/lat-1x.png" \
  "sh:$c; c 905 592" "sh:$form" "sh:$os 3; xdotool key Return" "sh:$c; c 1007 227" "sh:$track" \
  "sh:import -window root $OUT/lat-8x.png" "sh:$c; c 905 592"
