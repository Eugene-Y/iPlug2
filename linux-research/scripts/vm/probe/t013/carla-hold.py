#!/usr/bin/env python3
# Load one plug-in in Carla's backend (libcarla_standalone2, no Carla GUI), open its editor in Carla's X11 host
# window and keep the engine idling for <seconds>, so that another script can drag onto the editor meanwhile.
# Usage: carla-hold.py <carla-src-dir> <clap|vst3> <plugin-path> <clap-id or ""> <seconds>
# Needs a JACK server (carla-run.sh starts a dummy jackd). Plug-in stderr (iPlug traces) is this process's stderr.
import os, sys, time

carla, fmt, path, clapId, seconds = sys.argv[1:6]
sys.path.insert(0, os.path.join(carla, "source", "frontend"))
from carla_backend import *  # noqa: E402,F403

host = CarlaHostDLL(os.path.join(carla, "bin", "libcarla_standalone2.so"), False)
host.set_engine_option(ENGINE_OPTION_PATH_BINARIES, 0, os.path.join(carla, "bin"))
host.set_engine_option(ENGINE_OPTION_PATH_RESOURCES, 0, os.path.join(carla, "resources"))
print("engine", host.engine_init("JACK", "t013"), flush=True)
ptype = PLUGIN_CLAP if fmt == "clap" else PLUGIN_VST3
print("load", host.add_plugin(BINARY_NATIVE, ptype, path, "A", clapId, 0, None, 0), flush=True)
host.show_custom_ui(0, True)
end = time.time() + float(seconds)
while time.time() < end:
    host.engine_idle()
    time.sleep(0.02)
host.show_custom_ui(0, False)
host.remove_plugin(0)
print("engine-close", host.engine_close(), flush=True)
