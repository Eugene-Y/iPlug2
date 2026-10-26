#!/usr/bin/env bash
# T-017: IWebViewControl inside an IGraphics editor (IPlugOSCEditor) in REAPER. Runs INSIDE the GUI container.
# Usage: iwebviewcontrol-test.sh <outdir> <"CLAP: IPlugOSCEditor"> <plugin>
# The example's web view starts hidden; its button shows it; turning the knob sends an OSC message to itself,
# whose log the web view shows (LoadHTML). Then a corner-resizer rescale, hide again, close: no helper left.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
rpr() { "$HERE/scripts/rpr.sh" "$1"; }
helpers() { pgrep -fc '[/]iplug2-webview-helper' || true; }
shot() { import -window root "$OUT/$1.png"; }
"$HERE/scripts/reaper-up.sh" "$PLUG" > "$OUT/reaper-up.txt" 2>&1
RPID=$(cat /tmp/reaper.pid)
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 5
W=$(xdotool search --onlyvisible --name "IPlugOSCEditor.*Track 1" | tail -1)
read -r OX OY < <(xwininfo -id "$W" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }')
echo "editor at $OX,$OY; helpers $(helpers)"
shot w01-hidden
xdotool mousemove $((OX + 109)) $((OY + 386)) click 1; sleep 2
xdotool mousemove $((OX + 317)) $((OY + 310)) mousedown 1; for i in 1 2 3 4 5; do xdotool mousemove_relative 0 -6; sleep 0.1; done; xdotool mouseup 1; sleep 2
shot w02-shown-log
convert "$OUT/w02-shown-log.png" -crop "580x190+$((OX + 10))+$((OY + 397))" +repage "$OUT/w02-webview.png"
SD=$(convert "$OUT/w02-webview.png" -colorspace Gray -format '%[fx:standard_deviation]' info:)
awk -v s="$SD" 'BEGIN{exit !(s < 0.02)}' && fail "web view area blank after showing it (stddev $SD)" || echo "web view shows content (stddev $SD)"
# corner resizer (scale mode): drag the bottom-right corner
xdotool mousemove $((OX + 595)) $((OY + 595)) mousedown 1; for i in 1 2 3 4 5 6 7 8; do xdotool mousemove_relative 15 15; sleep 0.1; done; xdotool mouseup 1; sleep 2
shot w03-rescaled
xwininfo -root -tree | grep -c . > /dev/null
xdotool mousemove $((OX + 140)) $((OY + 460)) click 1; sleep 2
shot w04-hidden-again
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; sleep 3
echo "after close: helpers $(helpers)"; [ "$(helpers)" = 0 ] || fail "$(helpers) helpers after close"
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 4; shot w05-reopened
[ "$(helpers)" = 1 ] || fail "$(helpers) helpers after reopen"
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; sleep 3
st=$(ps -o stat= -p "$RPID" 2>/dev/null); [[ -n "$st" && "$st" != Z* ]] || fail "REAPER died"
cp /tmp/reaper.log "$OUT/reaper.log"
if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
