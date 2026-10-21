#!/usr/bin/env python3
# T-013: drive Carla's backend (libcarla_standalone2) with one plug-in, no Carla GUI.
# Usage: carla-probe.py <carla-src-dir> <clap|vst3> <plugin-path> <clap-id or ""> <outdir>
# Steps: load, editor open + screenshot, parameter dump, set a parameter from the host with the editor open,
# state save -> change -> load (value restored?), a second instance with its editor, editor close/reopen x5,
# remove the second instance with its editor open, remove the first. Plug-in stderr (IPLUG_HOSTLOOP traces)
# goes to this process's stderr. Prints "STEP <name> ok|FAIL ..." lines; exit 1 on any FAIL.
import os, subprocess, sys, time

carla, fmt, path, clapId, out = sys.argv[1:6]
sys.path.insert(0, os.path.join(carla, "source", "frontend"))
from carla_backend import *  # noqa: E402,F403

os.makedirs(out, exist_ok=True)
failed = []

def step(name, ok, detail=""):
    print(f"STEP {name} {'ok' if ok else 'FAIL'} {detail}", flush=True)
    if not ok:
        failed.append(name)

def idle(seconds):
    end = time.time() + seconds
    while time.time() < end:
        host.engine_idle()
        time.sleep(0.02)

def shot(name):
    subprocess.run(["import", "-window", "root", os.path.join(out, name + ".png")], check=False)

def windows():
    r = subprocess.run(["xdotool", "search", "--onlyvisible", "--name", "."], capture_output=True, text=True)
    names = set()
    for wid in r.stdout.split():
        n = subprocess.run(["xdotool", "getwindowname", wid], capture_output=True, text=True).stdout.strip()
        if n:
            names.add(n)
    return sorted(names)

host = CarlaHostDLL(os.path.join(carla, "bin", "libcarla_standalone2.so"), False)
host.set_engine_option(ENGINE_OPTION_PATH_BINARIES, 0, os.path.join(carla, "bin"))
host.set_engine_option(ENGINE_OPTION_PATH_RESOURCES, 0, os.path.join(carla, "resources"))
drivers = [host.get_engine_driver_name(i) for i in range(host.get_engine_driver_count())]
driver = "JACK"  # carla-run.sh starts a dummy jackd
step("engine", host.engine_init(driver, "t013"), f"driver={driver} of {drivers}")

ptype = PLUGIN_CLAP if fmt == "clap" else PLUGIN_VST3

def add(name):
    ok = host.add_plugin(BINARY_NATIVE, ptype, path, name, clapId, 0, None, 0)
    return ok, host.get_last_error()

ok, err = add("A")
step("load", ok, err)
if not ok:
    sys.exit(1)
idle(1)

info = host.get_plugin_info(0)
step("info", True, f"name={info['name']} maker={info['maker']} hints={info['hints']:#x}")

host.show_custom_ui(0, True)
idle(3)
shot("01-editor-open")
step("editor-open", any(n for n in windows() if "A" in n or info["name"] in n), f"windows={windows()}")

count = host.get_parameter_count(0)
names = []
for i in range(min(count, 400)):
    pinfo = host.get_parameter_info(0, i)
    names.append(pinfo["name"])
step("params", count > 0, f"count={count} first={names[:6]}")

target = next((i for i, n in enumerate(names) if n.lower() in ("main hz", "flt hz", "gain", "compression amnt")), 0)
before = host.get_current_parameter_value(0, target)
newValue = host.get_parameter_ranges(0, target)
lo, hi = newValue["min"], newValue["max"]
setTo = lo + (hi - lo) * 0.25 if abs(before - (lo + (hi - lo) * 0.25)) > 1e-6 else lo + (hi - lo) * 0.75
host.set_parameter_value(0, target, setTo)
idle(1.5)
shot("02-host-set-param")
now = host.get_current_parameter_value(0, target)
step("host-set-param", abs(now - setTo) < (hi - lo) * 1e-3, f"param={names[target]} {before:.4f} -> {now:.4f} (set {setTo:.4f})")

stateFile = os.path.join(out, "state.carxs")
step("state-save", host.save_plugin_state(0, stateFile), stateFile)
host.set_parameter_value(0, target, before)
idle(0.5)
step("state-load", host.load_plugin_state(0, stateFile), host.get_last_error())
idle(1)
restored = host.get_current_parameter_value(0, target)
step("state-restored", abs(restored - setTo) < (hi - lo) * 1e-3, f"{names[target]} = {restored:.4f}, saved {setTo:.4f}")

ok, err = add("B")
step("second-instance", ok, err)
host.show_custom_ui(1, True)
idle(3)
shot("03-two-editors")

for i in range(5):
    host.show_custom_ui(0, False)
    idle(0.6)
    host.show_custom_ui(0, True)
    idle(0.8)
shot("04-after-reopen")
step("reopen-x5", True, f"windows={windows()}")

step("remove-B-editor-open", host.remove_plugin(1), host.get_last_error())
idle(1)
host.show_custom_ui(0, False)
idle(0.5)
step("remove-A", host.remove_plugin(0), host.get_last_error())
idle(0.5)
step("engine-close", host.engine_close())
print("RESULT:", "PASS" if not failed else f"FAIL {failed}", flush=True)
sys.exit(1 if failed else 0)
