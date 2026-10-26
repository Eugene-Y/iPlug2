#!/usr/bin/env python3
"""A virtual multi-touch screen (uinput, python3-evdev) for touch tests on the VM, driven by commands on a FIFO.

Usage (root, for /dev/uinput):  sudo -n python3 vtouch.py <fifo> <screen-width> <screen-height> [<log>]
The device's range is the screen's (`xdpyinfo | grep dimensions`), so the X server maps its coordinates 1:1 onto
screen pixels. SIGINT or SIGTERM removes the device like 'quit'.
Commands, one or more per line, separated by ';':
  down <slot> <x> <y>    finger <slot> (0-9) touches at x,y
  move <slot> <x> <y>
  up <slot>
  syn                    end of one frame (SYN_REPORT); down/move/up take effect only then
  sleep <seconds>
  drag <ms> <steps> <slot> <x0> <y0> <x1> <y1> [<slot> <x0> <y0> <x1> <y1> ...]
                         touches every listed finger down at x0,y0, moves them together to x1,y1 in <steps> frames
                         over <ms>, and keeps them down (send 'up' afterwards)
  quit                   removes the device and exits
"""
import os
import signal
import sys
import time

from evdev import AbsInfo, UInput, ecodes as e

SLOTS = 10


def main():
    fifo, width, height = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    log = open(sys.argv[4], "a", buffering=1) if len(sys.argv) > 4 else sys.stderr
    if not os.path.exists(fifo):
        os.mkfifo(fifo)
        os.chmod(fifo, 0o666)  # mkfifo's mode goes through root's umask; the user writes the commands

    def leave(signum, frame):
        raise SystemExit(0)

    signal.signal(signal.SIGINT, leave)
    signal.signal(signal.SIGTERM, leave)

    def axis(maximum):
        return AbsInfo(value=0, min=0, max=maximum, fuzz=0, flat=0, resolution=10)

    capabilities = {
        e.EV_KEY: [e.BTN_TOUCH],
        e.EV_ABS: [
            (e.ABS_X, axis(width - 1)),
            (e.ABS_Y, axis(height - 1)),
            (e.ABS_MT_SLOT, AbsInfo(0, 0, SLOTS - 1, 0, 0, 0)),
            (e.ABS_MT_TRACKING_ID, AbsInfo(0, 0, 65535, 0, 0, 0)),
            (e.ABS_MT_POSITION_X, axis(width - 1)),
            (e.ABS_MT_POSITION_Y, axis(height - 1)),
        ],
    }
    ui = UInput(capabilities, name="t030 virtual touchscreen", input_props=[e.INPUT_PROP_DIRECT])
    log.write(f"device {ui.device.path if ui.device else '?'} ready\n")

    down = {}
    next_id = [1]

    def write_finger(slot, x, y):
        ui.write(e.EV_ABS, e.ABS_MT_SLOT, slot)
        if slot not in down:
            ui.write(e.EV_ABS, e.ABS_MT_TRACKING_ID, next_id[0])
            next_id[0] += 1
        ui.write(e.EV_ABS, e.ABS_MT_POSITION_X, x)
        ui.write(e.EV_ABS, e.ABS_MT_POSITION_Y, y)
        down[slot] = (x, y)

    def syn():
        if down:
            first = down[min(down)]
            ui.write(e.EV_ABS, e.ABS_X, first[0])
            ui.write(e.EV_ABS, e.ABS_Y, first[1])
        ui.write(e.EV_KEY, e.BTN_TOUCH, 1 if down else 0)
        ui.syn()

    def run(cmd):
        if not cmd:
            return True
        op, args = cmd[0], [int(float(a)) if a.lstrip("-").replace(".", "", 1).isdigit() else a for a in cmd[1:]]
        if op == "down" or op == "move":
            write_finger(*args)
        elif op == "up":
            ui.write(e.EV_ABS, e.ABS_MT_SLOT, args[0])
            ui.write(e.EV_ABS, e.ABS_MT_TRACKING_ID, -1)
            down.pop(args[0], None)
        elif op == "syn":
            syn()
        elif op == "sleep":
            time.sleep(float(cmd[1]))
        elif op == "drag":
            ms, steps, fingers = args[0], args[1], args[2:]
            fingers = [fingers[i:i + 5] for i in range(0, len(fingers), 5)]
            for step in range(steps + 1):
                for slot, x0, y0, x1, y1 in fingers:
                    write_finger(slot, round(x0 + (x1 - x0) * step / steps), round(y0 + (y1 - y0) * step / steps))
                syn()
                time.sleep(ms / 1000.0 / steps)
        elif op == "quit":
            return False
        else:
            log.write(f"unknown command {cmd}\n")
        return True

    try:
        while True:
            with open(fifo) as commands:
                for line in commands:
                    for cmd in line.split(";"):
                        log.write(f"{time.monotonic():.3f} {cmd.strip()}\n")
                        if not run(cmd.split()):
                            return
    finally:
        for slot in list(down):
            run(["up", str(slot)])
        syn()
        ui.close()
        log.write("device removed\n")


if __name__ == "__main__":
    main()
