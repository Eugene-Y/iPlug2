#!/usr/bin/env bash
# Case 5 in Qtractor (VST3), what the host does with the change: with Gneiss' editor closed, open its plug-in form,
# set "oversampling" to 1x and then 8x, close the form, stop and start the transport; all under call-trace.sh with
# latency.syms. Coordinates are those of t013vst3.qtr on the 1368x863 VM screen; the form opens at Page 2.
# Usage (vm-x.sh, Qtractor running, transport playing, no editor or form open): [SYMS=<file>] restart-seq.sh <out-file>
H=$(cd "$(dirname "$0")" && pwd)
c='c(){ xdotool mousemove $1 $2 mousedown 1; sleep 0.08; xdotool mouseup 1; sleep ${3:-1}; }'
os="xdotool mousemove 566 376 click 1 key ctrl+a type"
form="xdotool mousemove 395 259 mousedown 3; sleep 0.1; xdotool mouseup 3; sleep 1; $c; c 456 556 2"
STEP=4 PROC=qtractor bash "$H/../renoise/call-trace.sh" "$1" 45 "${SYMS:-$H/latency.syms}" \
  "sh:$form" "sh:$os 0; xdotool key Return" "sh:$os 3; xdotool key Return" "sh:$c; c 1007 227" \
  "sh:$c; c 411 158" "sh:$c; c 444 158"
