#!/usr/bin/env bash
# Stop the nested GNOME Shell of nested-start.sh: a polite close of its window, else the processes on its private bus;
# then remove what a killed Xwayland leaves behind. Usage: nested-stop.sh   (run through vm-x.sh)
R="${XDG_RUNTIME_DIR:?}/t013-nested"
NP=$(pgrep -f '^gnome-shell --nested' | head -1)
[ -n "$NP" ] || { echo "no nested shell"; rm -rf "$R"; exit 0; }
BUS=$(tr '\0' '\n' < /proc/$NP/environ | grep '^DBUS_SESSION_BUS_ADDRESS=')
# The nested Xwayland's public display; the shell's own DISPLAY is the outer one, whose files must stay.
XD=$(sed -n 's/.*Using public X11 display :\([0-9]*\).*/\1/p' "$R/shell.log" 2>/dev/null)
OUTER=$(tr '\0' '\n' < /proc/$NP/environ | sed -n 's/^DISPLAY=:\([0-9]*\).*/\1/p')
W=$(xdotool search --pid "$NP" 2>/dev/null | while read -r w; do xwininfo -id "$w" | grep -q IsViewable && echo "$w"; done | head -1)
[ -n "$W" ] && wmctrl -i -c "$W"
for i in $(seq 20); do kill -0 "$NP" 2>/dev/null || break; sleep 0.5; done
on_bus() { for p in $(pgrep -u "$(id -u)"); do { tr '\0' '\n' < /proc/$p/environ; } 2>/dev/null | grep -qxF "$BUS" && echo "$p"; done; }
if kill -0 "$NP" 2>/dev/null; then
  echo "close did not end it; stopping its processes"
  L=$(on_bus); [ -n "$L" ] && kill $L; sleep 2
  L=$(on_bus); [ -n "$L" ] && kill -9 $L; sleep 1
fi
pkill -f '^dbus-run-session -- gnome-shell --nested' 2>/dev/null
L=$(on_bus); [ -z "$L" ] && echo "nested shell stopped" || echo "still running: $L"
# Xwayland's public display and the one for managed services (:N+1), when a kill left their files. One Xwayland,
# started as :N, serves both.
if [ -n "$XD" ] && [ "$XD" != "$OUTER" ] && [ "$(( XD + 1 ))" != "$OUTER" ] \
   && ! pgrep -f "^/usr/bin/Xwayland :$XD " >/dev/null; then
  rm -f "/tmp/.X$XD-lock" "/tmp/.X11-unix/X$XD" "/tmp/.X$(( XD + 1 ))-lock" "/tmp/.X11-unix/X$(( XD + 1 ))"
fi
rm -rf "$R"
