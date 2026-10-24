# Sourced on the VM after vm-display-env: point X11 clients at the nested shell's Xwayland and Wayland display.
NP=$(pgrep -f '^gnome-shell --nested' | head -1)
export DISPLAY=:1 XAUTHORITY=$(ls /run/user/1000/.mutter-Xwaylandauth.* | head -1) WAYLAND_DISPLAY=wayland-0 XDG_SESSION_TYPE=wayland
NESTED_BUS=$(tr '\0' '\n' < /proc/$NP/environ | sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p')
nested_overview_off() {
  DBUS_SESSION_BUS_ADDRESS=$NESTED_BUS gdbus call --session --dest org.gnome.Shell --object-path /org/gnome/Shell \
    --method org.freedesktop.DBus.Properties.Set org.gnome.Shell OverviewActive "<false>"
}
