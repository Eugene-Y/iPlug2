# Sourced on the VM after vm-display-env: point X11 clients at the nested shell's Xwayland and Wayland display
# (nested-start.sh), and give nested_overview_off for its Activities overview.
NP=$(pgrep -f '^gnome-shell --nested' | head -1)
[ -n "$NP" ] || { echo "nested-env: no nested shell" >&2; return 1; }
NR=$(tr '\0' '\n' < /proc/$NP/environ | sed -n 's/^XDG_RUNTIME_DIR=//p')
OUTER_DISPLAY=$(tr '\0' '\n' < /proc/$NP/environ | sed -n 's/^DISPLAY=//p')
OUTER_XAUTHORITY=$(tr '\0' '\n' < /proc/$NP/environ | sed -n 's/^XAUTHORITY=//p')
export DISPLAY=$(sed -n 's/.*Using public X11 display \(:[0-9]*\).*/\1/p' "$NR/shell.log")
export XAUTHORITY=$(ls -t "$NR"/.mutter-Xwaylandauth.* | head -1) WAYLAND_DISPLAY="$NR/wayland-0" XDG_SESSION_TYPE=wayland
NESTED_BUS=$(tr '\0' '\n' < /proc/$NP/environ | sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p')
nested_overview_off() {
  DBUS_SESSION_BUS_ADDRESS=$NESTED_BUS gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell \
    --method org.freedesktop.DBus.Properties.Set org.gnome.Shell OverviewActive "<false>"
}
