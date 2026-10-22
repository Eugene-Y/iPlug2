#!/usr/bin/env python3
"""Write a Qtractor 1.5.3 session (.qtr) with one audio track per plug-in, from the session Qtractor saved on the VM
(evidence/t013/qtractor/t013vst3.qtr), the plug-ins' saved state and parameters dropped.
Usage: make-session.py <template.qtr> <out.qtr> <session-dir> <CLAP|VST3> <path-to-plugin-binary>...
  CLAP: the .clap file; VST3: the bundle's Contents/<arch>-linux/<Name>.so (what Qtractor stores)."""
import re, sys

template, out, directory, fmt = sys.argv[1:5]
paths = sys.argv[5:]
src = open(template).read()
src = re.sub(r"<directory>.*?</directory>", f"<directory>{directory}</directory>", src)
track = re.search(r"  <track type=\"audio\" name=\"Track 1\">.*?\n  </track>\n", src, re.S).group(0)
track = re.sub(r"\s*<configs>.*?</configs>", "", track, flags=re.S)
track = re.sub(r"\s*<params>.*?</params>", "", track, flags=re.S)
tracks = []
for i, path in enumerate(paths, 1):
    name = re.sub(r"\.(clap|so)$", "", path.rsplit("/", 1)[-1])
    t = track.replace('name="Track 1"', f'name="Track {i}"')
    t = re.sub(r'<plugin type="[A-Z0-9]+">', f'<plugin type="{fmt}">', t)
    t = re.sub(r"<filename>.*?</filename>", f"<filename>{path}</filename>", t)
    t = re.sub(r"<label>.*?</label>", f"<label>{name}</label>", t)
    tracks.append(t)
src = re.sub(r"  <track type=\"audio\".*\n  </track>\n", "".join(tracks), src, flags=re.S)
open(out, "w").write(src)
