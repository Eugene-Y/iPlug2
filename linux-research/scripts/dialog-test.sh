#!/usr/bin/env bash
# P4 dialogs in REAPER, through IPlugControls built with -DIPLUG_TEST_DIALOGS (its t-* buttons). Runs INSIDE the GUI
# container (Docker: zenity, no portal; the VM: IPLUG_LINUX_DIALOGS=zenity forces the same path there).
# Usage: dialog-test.sh <outdir> "<FMT: Name>" <plugin>
# Checks, each a FAIL line when it does not hold:
#   open / save (extension added) / save onto an existing name (our overwrite question) / folder / cancel each,
#   message boxes (Yes-No-Cancel by button and by Escape, OK with Cyrillic text, OK-Cancel), colour, the
#   synchronous call (empty at once), the stock disk preset manager's Load button (the chosen file loads and its folder
#   becomes the preset list; cancel keeps both), Cyrillic and spaces in every path, every handler on REAPER's main thread,
#   the editor closed with a dialog open (dialog gone, no handler, REAPER alive, dialogs work after reopening),
#   no GTK library added to REAPER's process while a dialog shows, the dialog program not REAPER's child.
# EXPECT_PORTAL=1 (the VM): file and folder dialogs must come from xdg-desktop-portal (no zenity while they show),
#   transient for REAPER's FX window; closing the editor must take the portal's dialog away too.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"
HERE="$(cd "$(dirname "$0")" && pwd)"
export LC_ALL=C.UTF-8 # xdotool type needs a UTF-8 locale for Cyrillic
mkdir -p "$OUT"; : > "$OUT/checks.txt"
FAILS=0
fail() { echo "FAIL: $*" | tee -a "$OUT/checks.txt"; FAILS=$((FAILS + 1)); }
pass() { echo "ok: $*" | tee -a "$OUT/checks.txt"; }

read -r _ OX OY < <("$HERE/dialog-up.sh" "$FX" "$PLUG")
RPID=$(cat /tmp/reaper.pid)
D="/tmp/dlg/Пресеты с пробелами"
echo "view origin $OX $OY, REAPER pid $RPID"

declare -A BUTTON=( [t-open]="759 628" [t-save]="889 628" [t-dir]="759 658" [t-ync]="889 658"
                    [t-ok]="759 688" [t-okc]="889 688" [t-color]="759 718" [t-sync]="889 718"
                    [preset-prev]="719 598" [preset-next]="769 598" [preset-load]="929 598" )
lines_seen=0
click() { read -r bx by <<<"${BUTTON[$1]}"; xdotool mousemove $((OX + bx)) $((OY + by)) click 1; }
shot() { import -window root "$OUT/$1.png"; }
alive() { local st; st=$(ps -o stat= -p "$RPID" 2>/dev/null); [[ -n "$st" && "$st" != Z* ]]; }
wait_window() { # <exact title>: waits for it, activates it, prints its id
  local w=""
  for _ in $(seq 50); do w=$(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null | tail -1); [ -n "$w" ] && break; sleep 0.2; done
  [ -n "$w" ] || { echo "window '$1' never appeared" | tee -a "$OUT/checks.txt" >&2; return 1; }
  sleep 0.8; xdotool windowactivate --sync "$w" 2>/dev/null; sleep 0.3; echo "$w"
}
new_lines() { grep -a '^IPLUG_TEST_DIALOG' /tmp/reaper.log | tail -n +$((lines_seen + 1)); }
expect() { # <label> <regex> [wait]: the next handler line matches, and it ran on the main thread
  local got=""
  for _ in $(seq $((${3:-5} * 5))); do got=$(new_lines); [ -n "$got" ] && break; sleep 0.2; done
  lines_seen=$((lines_seen + $(printf '%s' "$got" | grep -c '^IPLUG')))
  if [ -z "$got" ]; then fail "$1: no handler call"; return; fi
  if [ "$(printf '%s\n' "$got" | wc -l)" != 1 ]; then fail "$1: more than one handler call: $got"; return; fi
  grep -q ' main=1 ' <<<"$got" || fail "$1: handler off the main thread: $got"
  if grep -Eq -- "$2" <<<"$got"; then pass "$1: $got"; else fail "$1: '$got' does not match /$2/"; fi
}
expect_none() { # <label>: no handler line within 2 s
  sleep 2; local got; got=$(new_lines)
  if [ -n "$got" ]; then fail "$1: unexpected handler call: $got"; lines_seen=$((lines_seen + $(printf '%s\n' "$got" | wc -l))); else pass "$1: no handler call"; fi
}
# Typed text races GTK 4's inline completion in the location entry; a paste arrives in one piece.
paste_text() { printf "%s" "$1" | xclip -selection clipboard >/dev/null 2>&1; sleep 0.3; xdotool key ctrl+a ctrl+v; sleep 1; }
gtk_libs() { grep -Eo '/[^ ]*lib(gtk|gdk|adwaita|gtk4)[^ /]*\.so[^ ]*' /proc/$RPID/maps | sort -u; }
helpers() { pgrep -x zenity; pgrep -x kdialog; }

gtk_libs > "$OUT/gtk-libs-before.txt"
shot d00-start

# save: the name typed without extension gets .fxp
click t-save; w=$(wait_window "Save File") && {
  shot d01-save-dialog
  gtk_libs > "$OUT/gtk-libs-dialog-open.txt"
  if diff -q "$OUT/gtk-libs-before.txt" "$OUT/gtk-libs-dialog-open.txt" >/dev/null; then pass "no GTK library added to REAPER while the dialog shows ($(wc -l < "$OUT/gtk-libs-before.txt") GTK/GDK files are REAPER's own)"
  else fail "GTK libraries changed in REAPER's process: $(diff "$OUT/gtk-libs-before.txt" "$OUT/gtk-libs-dialog-open.txt" | tr '\n' ' ')"; fi
  if [ "${EXPECT_PORTAL:-0}" = 1 ]; then
    owner=$(ps -o args= -p "$(xdotool getwindowpid "$w" 2>/dev/null)" 2>/dev/null)
    transient=$(xprop -id "$w" WM_TRANSIENT_FOR 2>/dev/null | awk '{print $NF}')
    parentName=$(xdotool getwindowname "$transient" 2>/dev/null)
    [ -z "$(helpers)" ] && grep -q portal <<<"$owner" && pass "save dialog shown by '$owner', no zenity" || fail "save dialog owner '$owner', helpers: $(helpers | tr '\n' ' ')"
    grep -q "${FX#*: }.*Track 1" <<<"$parentName" && pass "dialog transient for $transient '$parentName'" || fail "WM_TRANSIENT_FOR '$transient' is '$parentName', not the FX window"
  else
    zp=$(pgrep -x zenity | head -1); pp=$(ps -o ppid= -p "$zp" | tr -d ' ')
    if [ -n "$zp" ] && [ "$pp" != "$RPID" ]; then pass "zenity pid $zp, parent $pp ($(ps -o comm= -p "$pp")), not REAPER"; else fail "zenity pid '$zp' parent '$pp'"; fi
  fi
  xdotool type --delay 50 "новый пресет"; sleep 0.3; xdotool key Return; }
expect save "save .* first='$D/новый пресет\.fxp' second='$D/'"

# save onto an existing file whose extension we add: our overwrite question, answered Yes
click t-save; w=$(wait_window "Save File") && { xdotool type --delay 50 "старый пресет"; sleep 0.3; xdotool key Return; }
sleep 2; q=$(wait_window "Save File") && { shot d02-overwrite-question; xdotool key Return; }
expect save-overwrite "save .* first='$D/старый пресет\.fxp'"

# open, by typing the name
click t-open; w=$(wait_window "Open File") && { shot d03-open-dialog; xdotool key ctrl+l; sleep 0.3; paste_text "старый пресет.fxp"; shot d03b-open-completed; xdotool key Return; }
expect open "open .* first='$D/старый пресет\.fxp' second='$D/'"

click t-open; w=$(wait_window "Open File") && xdotool key Escape
expect open-cancel "open .* first='' second=''"

# folder: the subfolder typed and accepted
click t-dir; w=$(wait_window "Choose a Directory") && { shot d04-dir-dialog; xdotool key ctrl+l; sleep 0.3; paste_text "подпапка два"; xdotool key Return; sleep 1; shot d04b-dir-after-return
    if xdotool search --onlyvisible --name "^Choose a Directory\$" >/dev/null 2>&1; then xdotool key alt+o; fi; }
expect dir "dir .* first='' second='$D/подпапка два/'"

click t-dir; w=$(wait_window "Choose a Directory") && xdotool key Escape
expect dir-cancel "dir .* first='' second=''"

# the stock disk preset manager (it lists the start folder): Load opens the file dialog; the handler loads the file and
# lists its folder, so ">" then steps through that folder
P="$D/подпапка два"
printf 'a' > "$P/пресет А.fxp"; printf 'b' > "$P/пресет Б.fxp"
click preset-load; w=$(wait_window "Open File") && { shot d09-preset-load-dialog; xdotool key ctrl+l; sleep 0.3; paste_text "$P/пресет А.fxp"; xdotool key Return; }
expect preset-load "preset .* first='$P/пресет А\.fxp'"
sleep 0.5; shot d10-preset-loaded
click preset-next
expect preset-next-in-chosen-folder "preset .* first='$P/пресет Б\.fxp'"
click preset-load; w=$(wait_window "Open File") && xdotool key Escape
expect_none preset-load-cancel
# "<" from Б gives А only if the list and its selection survived; a re-listed folder would wrap to Б
click preset-prev
expect preset-cancel-kept-folder "preset .* first='$P/пресет А\.fxp'"

# message boxes
click t-ync; w=$(wait_window "Dialog test") && {
  shot d05-msg-yesnocancel
  xdotool key Tab Return; } # focus moves from Yes to No (GTK 4 client-side shadows make clicking by geometry unreliable)
expect msg-no "msg-ync .*first='NO'"
click t-ync; w=$(wait_window "Dialog test") && xdotool key Escape
expect msg-escape "msg-ync .*first='CANCEL'"
click t-ok; w=$(wait_window "Dialog test") && { shot d06-msg-ok-cyrillic; xdotool key Return; }
expect msg-ok "msg-ok .*first='OK'"
click t-okc; w=$(wait_window "Dialog test") && xdotool key Escape
expect msg-okcancel "msg-okc .*first='CANCEL'"
click t-okc; w=$(wait_window "Dialog test") && xdotool key Return
expect msg-okcancel-ok "msg-okc .*first='OK'"

# colour: accepting the initial colour returns it
click t-color; w=$(wait_window "Dialog test colour") && { shot d07-color; xdotool key alt+s; }
expect color "color .*first='10,200,30,255'"

# no handler, no blocking: an empty answer at once
click t-sync
expect sync "sync .* first='' second=''" 2
grep -q 'without a completion handler cannot block' /tmp/reaper.log && pass "sync warning on stderr" || fail "no sync warning"

# the editor closes while a dialog shows: the dialog goes, no handler runs
for kind in t-save t-ync t-color preset-load; do
  click $kind; title="Save File"; [ $kind = t-ync ] && title="Dialog test"; [ $kind = t-color ] && title="Dialog test colour"
  [ $kind = preset-load ] && title="Open File"
  w=$(wait_window "$title") || { fail "close-mid-dialog $kind: no dialog"; continue; }
  "$HERE/rpr.sh" "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,2)" >/dev/null
  sleep 1.5
  if [ -n "$(helpers)" ]; then fail "close-mid-dialog $kind: dialog program still running: $(helpers | tr '\n' ' ')"; else pass "close-mid-dialog $kind: dialog program gone"; fi
  xdotool search --onlyvisible --name "^$title\$" >/dev/null 2>&1 && fail "close-mid-dialog $kind: a '$title' window is still showing" || pass "close-mid-dialog $kind: no '$title' window left"
  expect_none "close-mid-dialog $kind"
  alive && pass "close-mid-dialog $kind: REAPER alive" || { fail "REAPER died"; break; }
  "$HERE/rpr.sh" "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,3)" >/dev/null; sleep 2.5
done
shot d08-reopened
click t-open; w=$(wait_window "Open File") && xdotool key Escape
expect after-reopen "open .* first='' second=''"

zombies=$(ps -o pid=,stat= --ppid "$RPID" | awk '$2 ~ /^Z/' | wc -l)
[ "$zombies" = 0 ] && pass "no zombie children of REAPER" || fail "$zombies zombie children of REAPER"
alive && pass "REAPER alive at the end" || fail "REAPER dead at the end"

grep -a '^IPLUG_TEST_DIALOG\|iPlug2:' /tmp/reaper.log > "$OUT/reaper-dialog-lines.txt"
cp /tmp/reaper.log "$OUT/reaper-stderr.txt"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; grep FAIL "$OUT/checks.txt" >> "$OUT/RESULT.txt"; fi
[ "$FAILS" = 0 ]
