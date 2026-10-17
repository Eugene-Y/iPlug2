#!/usr/bin/env python3
"""T-026 F2: from the diagnostic build's log, every host echo (src=1) of comp amount that differs from the
latest MIDI-CC value (src=4) is a step back. It is harmless if a newer CC lands in the same block (< 1 ms
later: params are applied before MIDI). Otherwise its length is the time until the CC value is back.
Usage: f2-stepback.py <reaper-t026diag.txt>"""
import re
import sys

ev = []
for line in open(sys.argv[1], errors='replace'):
    m = re.search(r'T026DIAG\s+([\d.]+) change v=([\d.]+) src=(\d)', line)
    if m:
        ev.append((float(m.group(1)), float(m.group(2)), int(m.group(3))))
cc_value = None
echoes = stale = covered = 0
backs = []
for i, (t, v, src) in enumerate(ev):
    if src == 4:
        cc_value = v
    elif src == 1 and cc_value is not None:
        echoes += 1
        if v == cc_value:
            continue
        stale += 1
        nxt = next(((t2, v2) for t2, v2, s2 in ev[i + 1:] if v2 == cc_value or s2 == 4), None)
        if nxt and nxt[0] - t < 1.0:
            covered += 1
        else:
            backs.append((t, v, cc_value, (nxt[0] - t) if nxt else None))
print(f"host echoes: {echoes}; stale (differ from the latest CC): {stale}; "
      f"overridden by a newer CC in the same block: {covered}; audible step-backs: {len(backs)}")
for t, v, want, d in backs:
    print(f"  step back at {t:.3f} ms to {v:.4f} (CC said {want:.4f}) for {d if d is None else round(d, 3)} ms")
