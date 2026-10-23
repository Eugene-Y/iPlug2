#!/usr/bin/env bash
# REAPER, editor closed: which VST3 entries are called on which thread while idle (T-038 survey).
# Usage (GUI container): t038-survey2.sh <out> <plugin.vst3> [name]
set -uo pipefail
OUT="$1"; PLUG="$2"; NAME="${3:-IPlugEffect}"; B="$(cd "$(dirname "$0")" && pwd)"; HERE="$B/.."
mkdir -p "$OUT"
"$HERE/reaper-up.sh" "$PLUG" >/dev/null
RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,'VST3: $NAME',false,-1))"
sleep 3
echo "REAPER pid $RPID; idle, editor closed, 15 s" | tee "$OUT/allhits-idle.txt"
"$B/t038-allhits.sh" "$RPID" 15 '^\(iplug::IPlugVST3\|Steinberg::Vst::EditController\|Steinberg::Vst::EditControllerEx1\|Steinberg::Vst::SingleComponentEffect\|Steinberg::Vst::ComponentBase\)::' | tee -a "$OUT/allhits-idle.txt"
pkill -f '^/opt/REAPER/reaper' 2>/dev/null
