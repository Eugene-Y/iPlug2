#!/usr/bin/env bash
# T-030: a touch drag on IPlugControls' IPLUG_TEST_DRAGOUT square (APP built with -DIPLUG_TEST_DRAGOUT, staged in
# ~/iplug-builds/t030-drag) must not start a file drag; a third client can then grab the keyboard. Run through vm-x.sh.
set -u
O=/tmp/t030d; mkdir -p $O; FIFO=$O/touch.fifo; P=~/iplug-harness/linux-research/scripts/vm/probe; VT=$P/vtouch.py
gcc -O1 -o $O/kbgrab $P/kbgrab.c -lX11 || exit 1
echo hello > $O/drag.txt
rm -f $FIFO; mkfifo -m 666 $FIFO
sudo -n python3 $VT $FIFO $(xdpyinfo | awk "/dimensions/ { split(\$2, d, \"x\"); print d[1], d[2] }") $O/vtouch.log & VTPID=$!
sleep 2
xinput list | grep -i t030
IPLUG_TEST_DRAG_FILE=$O/drag.txt setsid ~/iplug-builds/t030-drag/iplug-apps/IPlugControls > $O/app.log 2>&1 & APID=$!
sleep 5
w=$(xdotool search --onlyvisible --name '^IPlugControls$' | tail -1)
[ -n "$w" ] || { echo "no window"; kill $APID; echo quit > $FIFO; exit 1; }
read -r OX OY < <(xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
echo "view origin $OX $OY"
SX=$((OX + 712)); SY=$((OY + 702)) # the 24x24 square at 700,690
xdotool mousemove $((OX + 900)) $((OY + 760)) mousedown 1 sleep 0.08 mouseup 1; sleep 0.5
echo "== $(date +%T.%N) finger down on the drag square, dragged, held"
echo "drag 600 10 0 $SX $SY $((SX - 200)) $((SY - 300))" > $FIFO; sleep 1.2
$O/kbgrab
import -window root $O/held.png
echo "up 0; syn" > $FIFO; sleep 1
echo "== $(date +%T.%N) finger up"
$O/kbgrab
import -window root $O/after-up.png
echo "== $(date +%T.%N) mouse click on IVKnobControl"
xdotool mousemove $((OX + 479)) $((OY + 420)) mousedown 1 sleep 0.08 mouseup 1; sleep 1; $O/kbgrab; xdotool mousedown 1 sleep 0.08 mouseup 1; sleep 1
$O/kbgrab
echo "== $(date +%T.%N) mouse drag from the drag square (must still start a drag), held"
xdotool mousemove $SX $SY mousedown 1; sleep 0.3
for d in 20 60 120; do xdotool mousemove $((SX - d)) $((SY - d)); sleep 0.15; done
$O/kbgrab
xdotool mouseup 1; sleep 0.8
echo "== $(date +%T.%N) mouse up"
$O/kbgrab
echo "== $(date +%T.%N) end"
xdotool key Escape; sleep 0.3
kill $APID 2>/dev/null; sleep 1; kill -9 $APID 2>/dev/null
echo quit > $FIFO; sleep 1; sudo -n pkill -TERM -f "^python3 .*vtouch.py $FIFO"; rm -f $FIFO
xinput list | grep -ci t030
echo "--- app.log"; cat $O/app.log
timeout 8 xdpyinfo >/dev/null && echo X-OK
