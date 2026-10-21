#!/usr/bin/env bash
# REAPER extension test (IPlugReaperExtension and its WebUI variant). Runs INSIDE the GUI container (gx.sh).
# Usage: reaper-ext-test.sh <outdir> <reaper_Name.so>
# Does: fresh REAPER with the extension; its actions are registered; Show/Hide opens the window and the editor paints;
#       (IGraphics example) the Add Track button adds a track and OnIdle updates the text; the floating window, docking,
#       a taller docker and undocking resize the editor with it, never drawn cut off; close/reopen x5; which thread
#       runs OnIdle, X events and drawing; the MsgBox action; REAPER quits cleanly with the window open.
# Env: REAPER_UI_SCALE (reaper-up.sh), GDB_COUNT_SECS (8; 0 skips the thread count).
set -uo pipefail
OUT="$1"; EXT="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
NAME=$(basename "$EXT" .so); NAME="${NAME#reaper_}"
mkdir -p "$OUT"
FAILS=()
fail() { echo "FAIL: $*"; FAILS+=("$*"); }
finish() {
  cp /tmp/reaper.log "$OUT/reaper-stderr.txt" 2>/dev/null
  if [ ${#FAILS[@]} -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL (${#FAILS[@]})"; printf '  - %s\n' "${FAILS[@]}"; fi | tee "$OUT/RESULT.txt"
  pkill -x reaper 2>/dev/null
  [ ${#FAILS[@]} -eq 0 ]; exit $?
}
rpr() { "$HERE/rpr.sh" "$1"; }
cmd_id() { echo "reaper.NamedCommandLookup(\"_$NAME: $1\")"; }
run_action() { rpr "reaper.Main_OnCommand($(cmd_id "$1"),0)" >/dev/null; }
toggle_state() { rpr "out(reaper.GetToggleCommandState($(cmd_id "$1")))"; }
SHOWHIDE="Action 3 - Show/Hide UI"; DOCK="Toggle dock UI"
alive() { kill -0 "$RPID" 2>/dev/null || { fail "REAPER died at '$1'"; finish; }; }
dialog_win() { xdotool search --onlyvisible --name "^$NAME Window\$" 2>/dev/null | head -1; }
editor_win() { # our IGraphics window: the X window of another client below the X bridge, the deepest of the largest ones under $1
  xwininfo -id "$1" -tree 2>/dev/null | awk '/^ +0x[0-9a-f]+ \(has no name\)/{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] >= best) { best=g[1]*g[2]; r=$1" "a[2]" "a[3]" "g[1]" "g[2] } } END { print (r ? r : "0 0 0 0 0") }'
}
client_size() { xwininfo -id "$1" | awk '/Width:/{w=$2} /Height:/{h=$2} END{print w" "h}'; }
# <png> <label> <X> <Y> <W> <H>: painted (not uniform) and not cut off (no black band: the window's background)
paint_check() {
  local sd black
  sd=$(convert "$1" -crop "${5}x${6}+${3}+${4}" +repage -colorspace Gray -format '%[fx:standard_deviation]' info:)
  black=$(convert "$1" -crop "${5}x${6}+${3}+${4}" +repage -colorspace Gray -threshold 2% -negate -format '%[fx:mean]' info:)
  if awk -v s="$sd" 'BEGIN{exit !(s < 0.02)}'; then fail "paint[$2]: editor ${5}x${6}+${3}+${4} blank (stddev $sd)"; else echo "paint[$2] ok (stddev $sd)"; fi
  if awk -v b="$black" 'BEGIN{exit !(b > 0.05)}'; then fail "paint[$2]: $(awk -v b="$black" 'BEGIN{printf "%.0f", b*100}')% of the editor is black (drawn cut off?)"; else echo "paint[$2] no black band (black fraction $black)"; fi
}
# <label> <png> <window>: the editor fills the client area of <window> and is painted
editor_check() {
  local id x y w h cw ch
  read -r id x y w h < <(editor_win "$3")
  read -r cw ch < <(client_size "$3")
  echo "editor[$1]: $id ${w}x${h}+$x+$y in a ${cw}x${ch} client"
  [ "$w" -gt 0 ] || { fail "editor[$1]: no editor window under $3"; return; }
  [ "$w" = "$cw" ] && [ "$h" = "$ch" ] || fail "editor[$1]: ${w}x${h}, client ${cw}x${ch}"
  # the web view's stand-in is SWELL's empty X bridge until T-017
  [ "$NAME" = IPlugReaperExtensionWebUI ] && return
  paint_check "$2" "$1" "$x" "$y" "$w" "$h"
}

REAPER_EXIT_FILE=/tmp/reaper.exit "$HERE/reaper-up.sh" "$EXT" > "$OUT/reaper-up.txt"
RPID=$(cat /tmp/reaper.pid)
rpr 'reaper.Main_SaveProjectEx(0, "/tmp/t031-empty.rpp", 0)' >/dev/null
rpr 'local i=0; while true do local id,name=reaper.kbd_enumerateActions(reaper.SectionFromUniqueID(0), i); if not id or id==0 then break end; if name:find("'"$NAME"': ",1,true)==1 then out(id, name) end; i=i+1 end' | tee "$OUT/actions.txt"
[ -s "$OUT/actions.txt" ] || fail "no '$NAME:' actions registered"

run_action "$SHOWHIDE"; sleep 2
W=$(dialog_win); [ -n "$W" ] || { fail "Show/Hide opened no '$NAME Window'"; finish; }
echo "window toggle: $(toggle_state "$SHOWHIDE")"
import -window root "$OUT/e01-open.png"; alive open
if [ "$NAME" = IPlugReaperExtensionWebUI ]; then
  echo "(the web view itself waits for T-017: only the window and its X bridge are checked)"
  editor_check open "$OUT/e01-open.png" "$W"
else
  editor_check open "$OUT/e01-open.png" "$W"
  read -r _ EX EY _ _ < <(editor_win "$W")
  before=$(rpr 'out(reaper.CountTracks(0))')
  xdotool mousemove $((EX + 150)) $((EY + 25)) click 1; sleep 1.5
  after=$(rpr 'out(reaper.CountTracks(0))')
  import -window root "$OUT/e02-add-track.png"
  echo "tracks $before -> $after"
  [ "$after" = $((before + 1)) ] || fail "Add Track button: tracks $before -> $after"
  # the NumTracks text (OnIdle -> ITextControl) changes
  if cmp -s <(convert "$OUT/e01-open.png" -crop "200x30+$((EX + 50))+$((EY + 135))" +repage rgb:-) \
            <(convert "$OUT/e02-add-track.png" -crop "200x30+$((EX + 50))+$((EY + 135))" +repage rgb:-); then
    fail "NumTracks text did not change after adding a track (OnIdle not running?)"
  fi
fi

xdotool windowsize "$W" 520 420; sleep 1.5
import -window root "$OUT/e03-resized.png"; alive resize
editor_check resized "$OUT/e03-resized.png" "$W"

run_action "$DOCK"; sleep 2
echo "dock toggle: $(toggle_state "$DOCK")"
MAIN=$(xdotool search --name '^(\[.*\] - )?REAPER v' | head -1)
import -window root "$OUT/e04-docked.png"; alive dock
read -r _ DX DY DW DH < <(editor_win "$MAIN")
echo "docked editor ${DW}x${DH}+$DX+$DY"
# grow the docker: drag its splitter, just above the editor, upwards
xdotool mousemove $((DX + 300)) $((DY - 3)) mousedown 1; sleep 0.2
for dy in 40 140 240 290; do xdotool mousemove $((DX + 300)) $((DY - 3 - dy)); sleep 0.2; done
xdotool mouseup 1; sleep 1.5
import -window root "$OUT/e05-docker-taller.png"
read -r _ DX2 DY2 DW2 DH2 < <(editor_win "$MAIN")
echo "docked editor after the splitter drag ${DW2}x${DH2}+$DX2+$DY2"
[ "$DH2" -gt "$DH" ] || fail "docked editor did not follow the docker: ${DW}x${DH} -> ${DW2}x${DH2}"
[ "$NAME" = IPlugReaperExtensionWebUI ] || paint_check "$OUT/e05-docker-taller.png" docked "$DX2" "$DY2" "$DW2" "$DH2"

run_action "$DOCK"; sleep 2
W=$(dialog_win)
import -window root "$OUT/e06-undocked.png"; alive undock
[ -n "$W" ] || fail "undocking opened no floating window"
[ -n "$W" ] && editor_check undocked "$OUT/e06-undocked.png" "$W"

for i in 1 2 3 4 5; do
  run_action "$SHOWHIDE"; sleep 0.7
  [ -z "$(dialog_win)" ] || fail "reopen $i: Show/Hide did not close the window"
  run_action "$SHOWHIDE"; sleep 1.2
  alive "reopen $i"
done
W=$(dialog_win)
import -window root "$OUT/e07-after-reopen.png"
[ -n "$W" ] && editor_check after-reopen "$OUT/e07-after-reopen.png" "$W"

if [ "${GDB_COUNT_SECS:-8}" != 0 ] && [ "$NAME" != IPlugReaperExtensionWebUI ]; then
  read -r _ EX EY EW EH < <(editor_win "$W")
  # drag the slider while counting, so that there is something to draw
  ( sleep $((${GDB_COUNT_SECS:-8} - 4)); xdotool mousemove $((EX + 30)) $((EY + 259)) mousedown 1
    for i in $(seq 0 20); do xdotool mousemove $((EX + 30 + 11 * i)) $((EY + 259)); sleep 0.1; done; xdotool mouseup 1 ) &
  "$HERE/gdb-count.sh" "$RPID" "${GDB_COUNT_SECS:-8}" iplug::ReaperExtBase::OnTimer iplug::igraphics::IGraphicsLinux::OnDisplayTimer \
    'iplug::igraphics::IGraphicsLinux::ProcessXEvents()' 'iplug::igraphics::IGraphics::Draw(iplug::igraphics::IRECTList&)' > "$OUT/threads-hits.txt" 2>&1
  wait
  echo "--- hits by gdb thread id (1 = REAPER main thread):"; cat "$OUT/threads-hits.txt"
  for f in OnTimer OnDisplayTimer 'IGraphics::Draw'; do grep -q "$f" "$OUT/threads-hits.txt" || fail "no $f hits"; done
  grep -v 'tid=1$' "$OUT/threads-hits.txt" | grep -q HIT && fail "work off REAPER's main thread: $(grep -v 'tid=1$' "$OUT/threads-hits.txt" | tr -s ' ' | tr '\n' ';')"
fi

if [ "$NAME" = IPlugReaperExtension ]; then
  run_action "Action 1 - MsgBox"; sleep 1.5
  import -window root "$OUT/e08-msgbox.png"
  MB=$(xdotool search --onlyvisible --name '^Reaper extension test$' | head -1)
  [ -n "$MB" ] || fail "Action 1 showed no message box"
  [ -n "$MB" ] && { xdotool windowactivate --sync "$MB" key Return 2>/dev/null; sleep 1; }
  [ -z "$(xdotool search --onlyvisible --name '^Reaper extension test$')" ] || fail "the message box did not close"
fi

alive "before quit"
# quit with the window open: ~ReaperExtBase then closes the editor and stops its timers during REAPER's shutdown
# the added tracks would raise a save prompt: reopen the empty project without one first
rpr 'reaper.Main_openProject("noprompt:/tmp/t031-empty.rpp"); reaper.Main_OnCommand(40004, 0); out("quit")' >/dev/null
for i in $(seq 50); do kill -0 "$RPID" 2>/dev/null || break; sleep 0.2; done
sleep 0.5
if kill -0 "$RPID" 2>/dev/null; then fail "REAPER still running 10 s after File: Quit"; else echo "REAPER exit status: $(cat /tmp/reaper.exit 2>/dev/null)"; fi
[ "$(cat /tmp/reaper.exit 2>/dev/null)" = 0 ] || fail "REAPER exit status $(cat /tmp/reaper.exit 2>/dev/null) after File: Quit"
grep -E 'SWELL API not found|iPlug2:|Segmentation|Aborted|terminate called' /tmp/reaper.log && fail "errors on stderr (above)"
finish
