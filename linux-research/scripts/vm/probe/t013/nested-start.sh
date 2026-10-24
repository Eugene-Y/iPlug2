#!/usr/bin/env bash
# Start a nested GNOME Shell (a Wayland compositor in an X11 window) with its own runtime directory, and wait until
# its Xwayland answers. Usage: nested-start.sh [WxH]   (run through vm-x.sh; then nested-env.sh, nested-stop.sh)
# The own XDG_RUNTIME_DIR keeps its wayland-0, Xwayland cookie, extension crash flag and accessibility bus socket out
# of the outer session's /run/user/<uid>: with the shared one, the nested accessibility bus took over at-spi/bus_0 and
# deleted it on exit, cutting the outer session's new clients off their accessibility bus.
SIZE="${1:-1280x768}"
R="$XDG_RUNTIME_DIR/t013-nested"
pgrep -f '^gnome-shell --nested' >/dev/null && { echo "a nested shell already runs"; exit 1; }
rm -rf "$R"; mkdir -m 700 "$R"
XDG_RUNTIME_DIR="$R" MUTTER_DEBUG_DUMMY_MODE_SPECS="$SIZE" \
  setsid dbus-run-session -- gnome-shell --nested --wayland > "$R/shell.log" 2>&1 < /dev/null &
END=$((SECONDS + 30))
while [ $SECONDS -lt $END ]; do
  D=$(sed -n 's/.*Using public X11 display \(:[0-9]*\).*/\1/p' "$R/shell.log")
  A=$(ls "$R"/.mutter-Xwaylandauth.* 2>/dev/null | head -1)
  [ -n "$D" ] && [ -n "$A" ] && DISPLAY="$D" XAUTHORITY="$A" timeout 1 xdpyinfo >/dev/null 2>&1 && { echo "nested up: $D"; exit 0; }
  sleep 0.5
done
# Seen in 4 of 7 starts: the shell never shows its panel and Xwayland waits on its Wayland socket for good.
echo "nested shell hung (no answer from Xwayland ${D:-?}); stop it with nested-stop.sh"; exit 1
