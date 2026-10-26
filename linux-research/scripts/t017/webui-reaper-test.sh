#!/usr/bin/env bash
# T-017: IPlugWebUI (WebView editor, out-of-process WebKitGTK helper) in REAPER. Runs INSIDE the GUI container.
# Usage: webui-reaper-test.sh <outdir> <"CLAP: IPlugWebUI"|"VST3: IPlugWebUI"> <plugin> [churn=20]
# Checks: render; knob drag -> host parameter (UI->host); host parameter -> knob (host->UI, C++->JS);
# page buttons -> C++ (JS->C++) -> editor resize; typing into a text field (a test field added to the INSTALLED
# copy of the page, Release builds only: Debug builds load the page from the source tree); host shortcut (space)
# while the web view is not focused; close/reopen N times with no helper or WebKit process left; helper killed
# while open (REAPER survives, the editor says so, a reopen recovers); two instances; thread census.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; CHURN="${4:-20}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
finish() {
  cp /tmp/reaper.log "$OUT/reaper.log" 2>/dev/null
  if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
  pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
  exit 0
}
rpr() { "$HERE/scripts/rpr.sh" "$1"; }
alive() { local st; st=$(ps -o stat= -p "$RPID" 2>/dev/null); if [[ -n "$st" && "$st" != Z* ]]; then echo "REAPER alive ($1)"; else fail "REAPER died at $1"; finish; fi; }
helpers() { pgrep -fc '[/]iplug2-webview-helper' || true; }
webkits() { pgrep -fc '[W]ebKit(Web|Network)Process' || true; }
win() { xdotool search --onlyvisible --name "IPlugWebUI.*Track $1" 2>/dev/null | tail -1; }
view_geom() { # X Y W H of the largest child of the FX window: the plug-in's parent window
  local w; w=$(win "$1"); [ -n "$w" ] || { echo "0 0 0 0"; return; }
  xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print (r ? r : "0 0 0 0") }'
}
shot() { import -window root "$OUT/$1.png"; }
crop() { convert "$OUT/$1.png" -crop "$4x$5+$(($2))+$(($3))" +repage "$OUT/$6.png"; }
stddev() { convert "$1" -colorspace Gray -format '%[fx:standard_deviation]' info:; }
render_check() { # <shot> <track>
  local X Y W H sd; read -r X Y W H < <(view_geom "$2")
  [ "$W" -ge 50 ] || { fail "render[$1]: no editor window for track $2"; return; }
  crop "$1" "$X" "$Y" "$W" "$H" "$1-view"; sd=$(stddev "$OUT/$1-view.png")
  if awk -v s="$sd" 'BEGIN{exit !(s < 0.02)}'; then fail "render[$1]: editor blank (stddev $sd)"; else echo "render[$1] ok (stddev $sd)"; fi
}
wait_helpers() { # <count> <secs>
  local i; for i in $(seq $(( $2 * 10 ))); do [ "$(helpers)" = "$1" ] && return 0; sleep 0.1; done; return 1
}
# The Gain parameter's REAPER index: VST3 lists the plug-in's bypass parameter first
GAIN_LUA="local gi=0; for i=0,reaper.TrackFX_GetNumParams(t,0)-1 do local _,n=reaper.TrackFX_GetParamName(t,0,i,''); if n=='Gain' then gi=i break end end"
param() { rpr "local t=reaper.GetTrack(0,0); $GAIN_LUA; out(string.format('%.4f', reaper.TrackFX_GetParamNormalized(t,0,gi)))"; }

PAGE=""
case "$PLUG" in
  *.clap) PAGE=/tmp/rhome/.clap/$(basename "${PLUG%.clap}").resources/web/index.html ;;
  *.vst3) PAGE=/tmp/rhome/.vst3/$(basename "$PLUG")/Contents/Resources/web/index.html ;;
esac
REAPER_WRAPPER="stdbuf -oL -eL" "$HERE/scripts/reaper-up.sh" "$PLUG" > "$OUT/reaper-up.txt" 2>&1
RPID=$(cat /tmp/reaper.pid)
# The text field for the typing check, in the installed copy only
if [ -f "$PAGE" ]; then
  sed -i 's|<input type="file" id="textfile" name="textfile" accept="text/\*" />|&<input type="text" id="t017text" size="14" style="margin-left:8px">|' "$PAGE"
  grep -q t017text "$PAGE" && echo "test field added to $PAGE"
fi

rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); out('add', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 6
shot r01-open; alive open; render_check r01-open 1
read -r OX OY OW OH < <(view_geom 1); echo "editor at $OX,$OY ${OW}x$OH"
[ "$(helpers)" = 1 ] || fail "helpers after open: $(helpers), expected 1"
echo "processes after open: helper $(helpers), WebKit $(webkits)"
ps -o rss=,args= -C iplug2-webview-helper,WebKitWebProcess,WebKitNetworkProcess > "$OUT/memory-one-editor.txt" 2>/dev/null

# UI -> host: drag the knob (beside its pointer line, which is not the drag target)
P0=$(param); xdotool mousemove $((OX + 104)) $((OY + 504)) mousedown 1
for i in 1 2 3 4 5 6; do xdotool mousemove_relative 0 -5; sleep 0.05; done; xdotool mouseup 1; sleep 0.5
P1=$(param); echo "knob drag: param $P0 -> $P1"
[ "$P0" != "$P1" ] || fail "knob drag did not change the host parameter ($P0)"
# host -> UI (C++ -> JS)
crop r01-open $((OX + 30)) $((OY + 410)) 140 170 knob-before
rpr "local t=reaper.GetTrack(0,0); $GAIN_LUA; reaper.TrackFX_SetParamNormalized(t,0,gi,0.75)" >/dev/null; sleep 1
shot r02-param; crop r02-param $((OX + 30)) $((OY + 410)) 140 170 knob-after
cmp -s "$OUT/knob-before.png" "$OUT/knob-after.png" && fail "knob did not move when the host set the parameter"
# Typing into the page's text field (Release: the field added above)
if grep -q t017text "$PAGE" 2>/dev/null && [ "${DEBUG_BUILD:-0}" = 0 ]; then
  xdotool mousemove $((OX + 437)) $((OY + 259)) click 1; sleep 0.5
  xdotool type --delay 60 "hello webview"; sleep 0.5
  shot r05-typed; crop r05-typed $((OX + 20)) $((OY + 230)) 560 60 typed
  echo "typed into the field: see typed.png"
  # the page must hold the keys: space typed above did not start REAPER's transport
  PS=$(rpr "out(reaper.GetPlayState())"); [ "$PS" = 0 ] || fail "space typed into the page reached the host (play state $PS)"
  # host shortcut with the web view unfocused: a click on REAPER's arrange view, then space
  M=$(xdotool search --name 'REAPER v[0-9]' | head -1); eval "$(xdotool getwindowgeometry --shell "$M")"
  xdotool mousemove $((X + WIDTH - 100)) $((Y + 300)) click 1; sleep 0.5; xdotool key space; sleep 1
  PS=$(rpr "out(reaper.GetPlayState())"); echo "play state after space in the arrange view: $PS"
  [ "$PS" != 0 ] || fail "space with the web view unfocused did not reach the host"
  rpr "reaper.OnStopButton()" >/dev/null
  rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 1
fi

# JS -> C++: Binary Test (Debug builds print it), Large / Small GUI (OnMessage -> Resize)
xdotool mousemove $((OX + 83)) $((OY + 96)) click 1; sleep 0.5
xdotool mousemove $((OX + 321)) $((OY + 147)) click 1; sleep 2
# a window manager may move the grown window to keep it on screen
shot r03-large; read -r OX OY LW LH < <(view_geom 1); echo "after Large GUI: ${LW}x$LH at $OX,$OY"
[ "$LW" -ge 1024 ] || fail "Large GUI: editor ${LW}x$LH, expected 1024 wide"
render_check r03-large 1
xdotool mousemove $((OX + 79)) $((OY + 147)) click 1; sleep 2
shot r04-small; read -r _ _ SW SH < <(view_geom 1); echo "after Small GUI: ${SW}x$SH"
[ "$SW" -le 520 ] || fail "Small GUI: editor ${SW}x$SH, expected 512 wide"
if grep -q "Data Size 4 bytes" /tmp/reaper.log; then echo "Binary Test reached C++ (Debug log)"; fi

# close / reopen: no helper or WebKit process may outlive its editor, and REAPER keeps no fd of theirs
: > "$OUT/churn.txt"
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; wait_helpers 0 5; sleep 1
FD0=$(ls /proc/"$RPID"/fd | wc -l)
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; wait_helpers 1 5
for i in $(seq "$CHURN"); do
  rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null
  wait_helpers 0 5 || fail "churn $i: $(helpers) helpers 5 s after close"
  echo "close $i: helpers $(helpers) webkit $(webkits)" >> "$OUT/churn.txt"
  rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null
  wait_helpers 1 5 || fail "churn $i: $(helpers) helpers after reopen"
  sleep 0.3
done
sleep 4; shot r06-after-churn; render_check r06-after-churn 1; alive churn
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; wait_helpers 0 5
sleep 2; W0=$(webkits); echo "after the last close: helpers $(helpers), WebKit $W0" | tee -a "$OUT/churn.txt"
[ "$W0" = 0 ] || fail "$W0 WebKit processes left after the last close"
FD1=$(ls /proc/"$RPID"/fd | wc -l); echo "REAPER fds with the editor closed: $FD0 before the churn, $FD1 after" | tee "$OUT/reaper-fds.txt"
[ "$FD1" -le "$FD0" ] || fail "REAPER has $((FD1 - FD0)) more fds after the churn"

# the helper killed while the editor is open
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 5
pkill -9 -f '[/]iplug2-webview-helper'; sleep 2
shot r07-killed; alive "helper killed"
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; sleep 1
rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 5
shot r08-reopened-after-kill; render_check r08-reopened-after-kill 1; alive "reopen after kill"
[ "$(helpers)" = 1 ] || fail "after the kill and a reopen: $(helpers) helpers"

# two instances, and the thread census while both are open
rpr "reaper.InsertTrackAtIndex(1,true); local t=reaper.GetTrack(0,1); out('add2', reaper.TrackFX_AddByName(t,\"$FX\",false,-1)); reaper.TrackFX_Show(t,0,3)"
sleep 6; shot r09-two; render_check r09-two 2; alive "two instances"
[ "$(helpers)" = 2 ] || fail "two editors: $(helpers) helpers"
ps -o rss=,args= -C iplug2-webview-helper,WebKitWebProcess,WebKitNetworkProcess > "$OUT/memory-two-editors.txt" 2>/dev/null
read -r QX QY _ _ < <(view_geom 2)
( sleep 4; xdotool mousemove $((QX + 104)) $((QY + 504)) mousedown 1; for i in $(seq 8); do xdotool mousemove_relative 0 -4; sleep 0.1; done; xdotool mouseup 1
  sleep 0.5; "$HERE/scripts/rpr.sh" "local t=reaper.GetTrack(0,1); $GAIN_LUA; reaper.TrackFX_SetParamNormalized(t,0,gi,0.3)" >/dev/null
  xdotool mousemove $((QX + 321)) $((QY + 147)) click 1 ) &
"$HERE/scripts/gdb-count.sh" "$RPID" 10 'iplug::IWebViewImpl::OnSocketReadable' 'iplug::IWebViewImpl::ProcessXEvents' \
  'iplug::IWebViewImpl::HandleMessage' 'iplug::IWebViewImpl::Flush' 'iplug::IPlugAPIBase::SendParameterValueFromUI' \
  'iplug::IPlugAPIBase::BeginInformHostOfParamChangeFromUI' 'iplug::IPlugAPIBase::EditorResizeFromUI' > "$OUT/threads.txt" 2>&1
wait
cat "$OUT/threads.txt"
grep -q HIT "$OUT/threads.txt" || fail "thread census: no hits"
if grep HIT "$OUT/threads.txt" | grep -qv 'tid=1$'; then fail "web view work off REAPER's main thread: $(grep -v 'tid=1$' "$OUT/threads.txt" | tr '\n' ';')"; fi
alive end
finish
