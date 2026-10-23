#!/usr/bin/env bash
# Which IPlugVST3 entries does REAPER call on which thread for a few Lua actions (editor closed)?
set -uo pipefail
OUT="$1"; PLUG="$2"; B="$(cd "$(dirname "$0")" && pwd)"; HERE="$B/.."
mkdir -p "$OUT"
"$HERE/reaper-up.sh" "$PLUG" >/dev/null
RPID=$(cat /tmp/reaper.pid)
"$HERE/rpr.sh" "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,'VST3: IPlugEffect',false,-1))"
sleep 2
RE='^\(iplug::IPlugVST3\|Steinberg::Vst::EditController\|Steinberg::Vst::EditControllerEx1\|Steinberg::Vst::SingleComponentEffect\|Steinberg::Vst::ComponentBase\)::'
act() { # name lua
  echo "=== $1 (main lwp=$RPID)" | tee -a "$OUT/actions.txt"
  "$B/t038-allhits.sh" "$RPID" 9 "$RE" > /tmp/ah.txt &
  sleep 6
  "$HERE/rpr.sh" "$2" | tee -a "$OUT/actions.txt"
  wait
  grep -v ' process$' /tmp/ah.txt | tee -a "$OUT/actions.txt"
}
act getparam "local t=reaper.GetTrack(0,0); out('get', reaper.TrackFX_GetParamNormalized(t,0,0))"
act setparam "local t=reaper.GetTrack(0,0); out('set', reaper.TrackFX_SetParamNormalized(t,0,0,0.3))"
act pdc "local t=reaper.GetTrack(0,0); out('pdc', reaper.TrackFX_GetNamedConfigParm(t,0,'pdc'))"
act play "reaper.OnPlayButton(); out('play')"
act stop "reaper.OnStopButton(); out('stop')"
act save "reaper.Main_SaveProject(0,false); out('save')"
pkill -f '^/opt/REAPER/reaper' 2>/dev/null
