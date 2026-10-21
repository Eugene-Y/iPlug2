# Print "<id> <x> <y> <w> <h>" (root coordinates) of the iPlug editor window inside the top-level named <name>:
# the XdndAware descendant (iPlug sets XdndAware on its own window). Also prints the descendants' XdndAware.
# Usage: editor-view.sh "<window name regex>"
for T in $(xdotool search --onlyvisible --name "$1"); do
  xprop -id "$T" WM_STATE 2>/dev/null | grep -q 'window state' || continue
  for c in $T $(xwininfo -tree -id "$T" | grep -o '^ *0x[0-9a-f]*'); do
    a=$(xprop -id "$c" XdndAware 2>&1 | head -1)
    g=$(xwininfo -id "$c" | awk '/Absolute upper-left X/{x=$4} /Absolute upper-left Y/{y=$4} /Width:/{w=$2} /Height:/{h=$2} END{print x, y, w, h}')
    echo "tree $T: $c $g ${a#*= }" >&2
    case "$a" in *"not found"*) ;; *) [ "$c" != "$T" ] && echo "$c $g";; esac
  done
done
