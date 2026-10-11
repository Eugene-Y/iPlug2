#!/usr/bin/env bash
# Editor open in REAPER when libGL.so.1 is unusable (an empty stand-in first on LD_LIBRARY_PATH).
# The plug-in must load and run; the editor must fail to open and the host must be told (CLAP set_parent /
# VST3 attached return false), so REAPER shows its generic UI. Runs INSIDE the GUI container.
# Usage: no-libgl-probe.sh <outdir> "<FMT: Name>" <plugin>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT" /tmp/fakegl
echo 'int iplug_fake_gl;' > /tmp/fakegl/fake.c
gcc -shared -fPIC -Wl,-soname,libGL.so.1 -o /tmp/fakegl/libGL.so.1 /tmp/fakegl/fake.c
LD_LIBRARY_PATH=/tmp/fakegl "$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)" | tee "$OUT/rpr.txt"
sleep 3
import -window root "$OUT/no-libgl.png"
grep -m1 libGL "/proc/$RPID/maps" | awk '{print "libGL mapped:", $NF}' | tee "$OUT/maps.txt"
grep -E "iPlug2" /tmp/reaper.log | sort | uniq -c | tee "$OUT/stderr-iplug.txt"
st=$(ps -o stat= -p "$RPID"); [[ -n "$st" && "$st" != Z* ]] && echo "REAPER ALIVE" | tee -a "$OUT/stderr-iplug.txt"
xwininfo -root -tree | grep -A6 "${FX#*: }.*Track 1" > "$OUT/xwininfo.txt"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null; true
