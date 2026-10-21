# Print the window chain under root point x,y, deepest first: id, size, WM_CLASS/name, XdndAware, XdndProxy.
# Tells who a file manager's drag would address (GTK/Qt sources look for XdndAware on the top-level client window).
# Usage: drop-chain.sh <x> <y>
xdotool mousemove "$1" "$2"; sleep 0.3
eval "$(xdotool getmouselocation --shell)"
W=$WINDOW
for i in $(seq 1 12); do
  info=$(xwininfo -id "$W" 2>/dev/null)
  [ -n "$info" ] || break
  geo=$(echo "$info" | awk '/Width:/{w=$2} /Height:/{h=$2} END{print w"x"h}')
  name=$(xprop -id "$W" WM_NAME 2>/dev/null | sed -n 's/^WM_NAME([^)]*) = //p' | cut -c1-40)
  cls=$(xprop -id "$W" WM_CLASS 2>/dev/null | sed -n 's/^WM_CLASS([^)]*) = //p')
  aware=$(xprop -id "$W" XdndAware 2>/dev/null | grep -q 'not found' && echo - || xprop -id "$W" -notype 32c XdndAware 2>/dev/null | sed 's/^XdndAware = /v/')
  proxy=$(xprop -id "$W" XdndProxy 2>/dev/null | grep -q 'not found' && echo - || xprop -id "$W" XdndProxy | sed 's/.*# //')
  state=$(xprop -id "$W" WM_STATE 2>/dev/null | grep -q 'window state' && echo WM_STATE || echo -)
  printf '%s %s name=%s class=%s aware=%s proxy=%s %s\n' "$W" "$geo" "${name:--}" "${cls:--}" "$aware" "$proxy" "$state"
  P=$(xwininfo -children -id "$W" 2>/dev/null | awk '/Parent window id:/{print $4}')
  R=$(xwininfo -root 2>/dev/null | awk '/Window id:/{print $4}')
  [ -z "$P" ] || [ "$P" = "$R" ] && break
  W=$P
done
