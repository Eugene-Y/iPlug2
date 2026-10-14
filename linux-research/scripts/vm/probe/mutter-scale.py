#!/usr/bin/env python3
"""T-007: set the monitor's scale through Mutter's DisplayConfig D-Bus API, as GNOME Settings does (temporary config).
Fractional scales on X11 need Mutter's experimental feature 'x11-randr-fractional-scaling'; this script enables it when
asked for a fractional scale. Usage: mutter-scale.py <scale> [mode id, e.g. 1920x1440@60.000] | --state"""
import subprocess
import sys

from gi.repository import Gio, GLib

FEATURE = "x11-randr-fractional-scaling"


def proxy():
    return Gio.DBusProxy.new_for_bus_sync(Gio.BusType.SESSION, Gio.DBusProxyFlags.NONE, None,
                                          "org.gnome.Mutter.DisplayConfig", "/org/gnome/Mutter/DisplayConfig",
                                          "org.gnome.Mutter.DisplayConfig", None)


def state():
    serial, monitors, logical, props = proxy().call_sync("GetCurrentState", None, Gio.DBusCallFlags.NONE, -1, None).unpack()
    current = []
    for (connector, *_), modes, _mprops in monitors:
        for mode_id, w, h, rate, preferred_scale, scales, mode_props in modes:
            if mode_props.get("is-current"):
                current.append((connector, mode_id, w, h, scales))
    return serial, current, logical, props


def mode_scales(wanted):
    _serial, monitors, _logical, _props = proxy().call_sync("GetCurrentState", None, Gio.DBusCallFlags.NONE, -1, None).unpack()
    for _ids, modes, _mprops in monitors:
        for mode_id, _w, _h, _rate, _preferred, scales, _props in modes:
            if mode_id == wanted:
                return scales
    sys.exit(f"no mode {wanted}")


def set_features(features):
    subprocess.run(["gsettings", "set", "org.gnome.mutter", "experimental-features", str(features)], check=True)


def main():
    if sys.argv[1:] == ["--state"]:
        serial, current, logical, props = state()
        print("modes:", current)
        print("logical monitors (x, y, scale, ...):", [(l[0], l[1], l[2]) for l in logical])
        print("layout-mode:", props.get("layout-mode"), "supports-changing-layout-mode:", props.get("supports-changing-layout-mode"))
        return
    scale = float(sys.argv[1])
    if scale != int(scale):
        features = subprocess.run(["gsettings", "get", "org.gnome.mutter", "experimental-features"],
                                  capture_output=True, text=True).stdout.strip()
        if FEATURE not in features:
            set_features([FEATURE])
            GLib.usleep(2_000_000)
    serial, current, logical, props = state()
    connector, mode_id, w, h, scales = current[0]
    if len(sys.argv) > 2:
        mode_id = sys.argv[2]
        scales = mode_scales(mode_id)
    match = [s for s in scales if abs(s - scale) < 0.01]
    if not match:
        sys.exit(f"scale {scale} not offered for {mode_id}; offered: {scales}")
    config = GLib.Variant("(uua(iiduba(ssa{sv}))a{sv})",
                          (serial, 1, [(0, 0, match[0], 0, True, [(connector, mode_id, {})])], {}))
    proxy().call_sync("ApplyMonitorsConfig", config, Gio.DBusCallFlags.NONE, -1, None)
    GLib.usleep(3_000_000)
    serial, current, logical, props = state()
    print("applied:", [(l[0], l[1], l[2]) for l in logical], "mode:", current[0][1])


main()
