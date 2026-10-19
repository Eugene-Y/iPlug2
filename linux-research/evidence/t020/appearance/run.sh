#!/usr/bin/env bash
# T-020: appearance from a fake portal, in the GUI container (REAPER, IPlugControls built with IPLUG_TEST_APPEARANCE).
# Usage: run.sh <out> <IPlugControls.clap|.vst3> "<FMT: Name>"
set -u
OUT="$1"; P="$2"; FX="$3"; HERE="$(cd "$(dirname "$0")" && pwd)"; R="$(cd "$HERE/../../../scripts" && pwd)"
mkdir -p "$OUT"
gcc "$HERE/fake-portal.c" $(pkg-config --cflags --libs dbus-1) -o /tmp/fake-portal || exit 2
pkill -f /tmp/fake-portal; export DBUS_SESSION_BUS_ADDRESS=$(cat /tmp/dbus.addr)
"$R/reaper-up.sh" "$P"
if [ "${LATE_PORTAL:-0}" = 1 ]; then
  # the portal starts only after the editor is open: its first value must still arrive (NameOwnerChanged)
  ( sleep 4; /tmp/fake-portal 1 > "$OUT/portal.txt" 2>&1 ) &
else
  # dark when the editor opens, light 5 s later, dark again 4 s after that
  /tmp/fake-portal 1 5 2 4 1 > "$OUT/portal.txt" 2>&1 &
  sleep 0.5
fi
"$R/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 12
grep -a 'IPLUG_TEST_APPEARANCE' /tmp/reaper.log | tee "$OUT/appearance.txt"
pkill -x reaper; pkill -f /tmp/fake-portal
cat "$OUT/portal.txt"
