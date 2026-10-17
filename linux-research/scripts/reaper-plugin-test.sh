#!/usr/bin/env bash
# End-to-end editor test in REAPER. Runs INSIDE the GUI container (use gx.sh / run-gui-test.sh).
# Usage: reaper-plugin-test.sh <outdir> <FXNAME e.g. "VST3: IPlugControls"> <plugin-bundle> [steps-file]
# Does: fresh REAPER with the plugin, add to track 1, open floating editor, screenshot,
#       optional xact steps (origin = editor view), close/reopen x5, second instance on track 2,
#       thread census + which thread runs OnDisplayTimer/Draw.
# Fails loudly (exit 1, "FAIL:" lines, RESULT line at the end) instead of passing vacuously:
#   - editor area not painted (near-uniform pixels) on first open / after reopen / 2nd instance
#   - steps: no repaint between c00 and c02 (identical screenshots)
#   - REAPER main PID died, or a helper `reaper -nonewinst` turned into a fresh instance
#   - any LUA ERROR from rpr.sh
#   - no Draw/OnDisplayTimer hits observed
# Env: GDB_CATCH_EARLY=1 attaches the crash catcher before the editor opens (catches crashes on
#      open, but the ptrace attach perturbs startup timing — it made PR #1336's timer-start race
#      lose almost every time). Default: attach right after the first open.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; STEPS="${4:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"; : > "$OUT/xact-fails.txt"
FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
finish() {
  tail -20 /tmp/reaper.log > "$OUT/reaper-stderr-tail.txt"
  if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
  pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
  [ ${#FAILS[@]} -eq 0 ]; exit $?
}
RPID=""
alive() {
  local st; st=$(ps -o stat= -p "$RPID" 2>/dev/null)
  if [[ -n "$st" && "$st" != Z* ]]; then echo "REAPER ALIVE pid=$RPID ($1)"; else fail "REAPER pid $RPID dead at '$1' (see gdb-crash.txt); later phases skipped"; sleep 2; finish; fi
  # rpr.sh starts `reaper -nonewinst`; if the main instance is gone it becomes a fresh REAPER and stays
  if pgrep -f '^/opt/REAPER/reaper -nonewinst' >/dev/null; then fail "a fresh REAPER instance replaced pid $RPID at '$1'"; fi
}
rpr() { local r; r=$("$HERE/rpr.sh" "$1"); echo "$r"; if grep -q 'LUA ERROR\|no output from lua' <<<"$r"; then fail "lua: $r"; fi; }
view_geom() { # "X Y W H" of the largest child (the plugin's embed container) of the FX window
  local w; w=$(xdotool search --onlyvisible --name "$1" | tail -1)
  [ -n "$w" ] || { echo "0 0 0 0"; return; }
  xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3]" "g[1]" "g[2] } } END { print (r ? r : "0 0 0 0") }'
}
render_check() { # <png> <label> <window-regex>: the editor area must not be (near-)uniform
  local g; read -r X Y W H < <(view_geom "$3")
  if [ "$W" -lt 50 ]; then fail "render[$2]: editor window '$3' not found"; return; fi
  local sd; sd=$(convert "$1" -crop "${W}x${H}+${X}+${Y}" +repage -colorspace Gray -format '%[fx:standard_deviation]' info:)
  if awk -v s="$sd" 'BEGIN{exit !(s < 0.02)}'; then fail "render[$2]: editor area ${W}x${H}+${X}+${Y} is blank (stddev $sd)"; else echo "render[$2] ok (stddev $sd)"; fi
}
start_catch() { ( "$HERE/gdb-catch.sh" "$RPID" "$OUT/gdb-crash.txt" & ); sleep 2; }

"$HERE/reaper-up.sh" "$PLUG"
RPID=$(cat /tmp/reaper.pid)
[ "${GDB_CATCH_EARLY:-0}" = 1 ] && start_catch
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); local i=reaper.TrackFX_AddByName(t,\"$FX\",false,-1); out('add',i); reaper.TrackFX_Show(t,0,3)"
sleep 3
import -window root "$OUT/r01-open.png"; alive open
render_check "$OUT/r01-open.png" first-open "${FX#*: }.*Track 1"
[ "${GDB_CATCH_EARLY:-0}" = 1 ] || start_catch
read -r OX OY _ _ < <(view_geom "${FX#*: }.*Track 1")
echo "view origin $OX $OY"
if [ -n "$STEPS" ]; then
  (echo "origin $OX $OY"; sed "s/__PROC__/$RPID/" "$STEPS") | OUT="$OUT" XACT_FMT="${FX%%:*}" "$HERE/xact.sh"
  alive steps
  if [ -s "$OUT/xact-fails.txt" ]; then while read -r l; do fail "steps: $l"; done < "$OUT/xact-fails.txt"; fi
  if [ -f "$OUT/c00-start.png" ] && [ -f "$OUT/c02-xy-keyboard.png" ] && cmp -s "$OUT/c00-start.png" "$OUT/c02-xy-keyboard.png"; then
    fail "steps: c00 and c02 identical -- editor never repainted after drag/click"
  fi
fi
for i in 1 2 3 4 5; do
  rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,2)" >/dev/null; sleep 0.7
  rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 1.2
  alive "reopen $i"
done
import -window root "$OUT/r02-after-reopen.png"
render_check "$OUT/r02-after-reopen.png" after-reopen "${FX#*: }.*Track 1"
rpr "reaper.InsertTrackAtIndex(1,true); local t=reaper.GetTrack(0,1); local i=reaper.TrackFX_AddByName(t,\"$FX\",false,-1); out('add2',i); reaper.TrackFX_Show(t,0,3)"
sleep 3
import -window root "$OUT/r03-two-instances.png"; alive "two instances"
render_check "$OUT/r03-two-instances.png" second-instance "${FX#*: }.*Track 2"
xwininfo -root -tree > "$OUT/xwininfo-tree.txt"
pkill -INT -f "gdb -q -batch -p $RPID" 2>/dev/null; sleep 1
"$HERE/threads.sh" "$RPID" 10 > "$OUT/threads.txt" 2>&1
# Hover across the 2nd instance's editor while counting, so that a UI without animations still has dirty controls to draw.
read -r PX PY PW PH < <(view_geom "${FX#*: }.*Track 2")
( sleep 1; for i in $(seq 0 19); do xdotool mousemove $((PX + PW * i / 20)) $((PY + PH * (i % 3 + 1) / 4)); sleep 0.2; done ) &
"$HERE/gdb-count.sh" "$RPID" 5 iplug::igraphics::IGraphicsLinux::OnDisplayTimer 'iplug::igraphics::IGraphicsLinux::ProcessXEvents()' 'iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)' > "$OUT/draw-threads.txt" 2>&1
wait
echo "--- draw/timer hits by gdb thread id (1 = REAPER main thread):"; cat "$OUT/draw-threads.txt"
[ -s "$OUT/draw-threads.txt" ] || fail "no Draw/OnDisplayTimer/ProcessXEvents hits in 5 s (editor not rendering, or REAPER gone)"
grep -q 'IGraphics::Draw' "$OUT/draw-threads.txt" || fail "no Draw hits in 5 s, even with the mouse moving over the editor"
# host-driven editors (P3 on): X events and drawing only on the host main thread
if grep -q 'ProcessXEvents' "$OUT/draw-threads.txt" && grep -v 'tid=1$' "$OUT/draw-threads.txt" | grep -q HIT; then
  fail "editor work off REAPER's main thread: $(grep -v 'tid=1$' "$OUT/draw-threads.txt" | tr -s ' ' | tr '\n' ';')"
fi
# PR #1336-style IPlugTimer: one Timer_impl pthread per plugin instance drives OnIdle/ISender;
# a thread that lost the start race leaves that instance without OnIdle while the editor may still paint.
SO=$PLUG; [ -d "$PLUG" ] && SO=$(find "$PLUG/Contents" -name '*.so' | head -1)
if nm -C "$SO" 2>/dev/null | grep -q 'Timer_impl::ThreadProc'; then
  NT=$(grep -c 'Timer_impl::ThreadProc' "$OUT/threads.txt")
  if [ "$NT" -lt 2 ]; then fail "IPlugTimer: $NT Timer_impl threads for 2 instances (an instance has no OnIdle)"; else echo "IPlugTimer threads ok ($NT)"; fi
fi
alive end
finish
