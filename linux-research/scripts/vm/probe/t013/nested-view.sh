#!/usr/bin/env bash
# Print the plug-in view's origin and size in the OUTER X11 screen's coordinates, for xdotool on the outer display
# (input that goes through the nested compositor). Usage: nested-view.sh "<FX window name regex>"   (through vm-x.sh)
. "$(dirname "$0")/nested-env.sh" || exit 1
# REAPER's FX window, not the frame mutter puts around it (the frame carries the same title).
W=$(for w in $(xdotool search --onlyvisible --name "$1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
[ -n "$W" ] || { echo "no window"; exit 1; }
# The plug-in's view is the largest window inside it.
read -r VX VY VW VH < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }')
# The nested compositor's output fills its viewable window on the outer display.
outer() { DISPLAY="$OUTER_DISPLAY" XAUTHORITY="$OUTER_XAUTHORITY" "$@"; }
SW=$(outer xdotool search --pid "$NP" | while read -r w; do outer xwininfo -id "$w" | grep -q IsViewable && echo "$w"; done | head -1)
[ -n "$SW" ] || { echo "no nested window"; exit 1; }
read -r OX OY < <(outer xwininfo -id "$SW" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}')
echo "view $((VX+OX)) $((VY+OY)) $VW $VH (nested $VX $VY, offset $OX $OY)"
