#!/usr/bin/env python3
"""F2 on mac REAPER without GUI: put a MIDI CC binding (CC <cc> -> param <pid>, full range, any channel) into the
Sand AU state of a REAPER project, i.e. what Sand's own MIDI learn would store.
The RPP <AU ...> block is base64 of: REAPER's header, a 4-byte length, the AU ClassInfo XML plist, a trailer.
The plist's "data" is iPlug's chunk: version hex (int32), then the hvoya mapper (size_t count = 0 when empty).
Usage: f2-au-inject.py <in.RPP> <out.RPP> [cc=1] [pid=0]"""
import base64
import plistlib
import re
import struct
import sys

src, dst = sys.argv[1], sys.argv[2]
cc = int(sys.argv[3]) if len(sys.argv) > 3 else 1
pid = int(sys.argv[4]) if len(sys.argv) > 4 else 0
text = open(src).read()
m = re.search(r'(<AU "[^"]*Sand[^"]*"[^\n]*\n)((?:\s+[A-Za-z0-9+/=]+\n)+)(\s+>)', text)
assert m, "no Sand AU block"
indent = re.match(r'\s+', m.group(2)).group(0)
blob = base64.b64decode(''.join(m.group(2).split()))
start = blob.index(b'<?xml')
(length,) = struct.unpack_from('<I', blob, start - 4)
info = plistlib.loads(blob[start:start + length])
chunk = info['data']
assert chunk[4:12] == b'\0' * 8, "CC map not empty"
binding = struct.pack('<Q', 1) + struct.pack('<i', cc) + struct.pack('<Q', 1) \
    + struct.pack('<ii', pid, cc) + struct.pack('<dd', 0.0, 1.0) + struct.pack('<i', 0)
info['data'] = chunk[:4] + binding + chunk[12:]
xml = plistlib.dumps(info, fmt=plistlib.FMT_XML)
blob = blob[:start - 4] + struct.pack('<I', len(xml)) + xml + blob[start + length:]
b64 = base64.b64encode(blob).decode()
lines = ''.join(indent + b64[i:i + 128] + '\n' for i in range(0, len(b64), 128))
open(dst, 'w').write(text[:m.start(2)] + lines + text[m.end(2):])
print(f"version 0x{struct.unpack_from('<I', chunk)[0]:08x}; bound CC {cc} -> param {pid}; plist {length} -> {len(xml)} bytes")
