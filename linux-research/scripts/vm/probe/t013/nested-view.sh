# Print the plug-in view's origin and size in the OUTER X11 screen's coordinates (:0), for xdotool on :0 (input that
# goes through the nested compositor). Usage: nested-view.sh "<FX window name regex>"   (run through vm-x.sh)
. "$(dirname "$0")/nested-env.sh"
W=$(for w in $(xdotool search --onlyvisible --name "$1"); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1)
[ -n "$W" ] || { echo "no window"; exit 1; }
read -r VX VY VW VH < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print r }')
# The nested compositor's output sits inside its X11 window on :0.
read -r OX OY < <(DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority xwininfo -id "$(DISPLAY=:0 XAUTHORITY=/run/user/$(id -u)/gdm/Xauthority xdotool search --pid "$(pgrep -f '^gnome-shell --nested' | head -1)" | head -1)" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}')
echo "view $((VX+OX)) $((VY+OY)) $VW $VH (nested $VX $VY, offset $OX $OY)"
