#!/usr/bin/env bash
# REAPER RSS while one editor stays open (does memory grow per frame or per open/close?), plus CPU of REAPER and Xorg.
# Usage: rss-open.sh <out> "<FMT: Name>" <plugin> [seconds=60]
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; S="${4:-60}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 5
for i in $(seq 0 10 "$S"); do echo "t=$i rss_kb=$(awk '/VmRSS/{print $2}' /proc/$RPID/status)"; sleep 10; done | tee "$OUT/rss.txt"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
