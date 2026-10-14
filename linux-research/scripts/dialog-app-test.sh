#!/usr/bin/env bash
# The standalone APP (GLib host loop): a save dialog, a message box and the editor's window closed with a dialog
# open. Runs INSIDE the GUI container. Usage: dialog-app-test.sh <outdir> <app binary>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
HERE="$(cd "$(dirname "$0")" && pwd)"
export LC_ALL=C.UTF-8
mkdir -p "$OUT"; : > "$OUT/checks.txt"
FAILS=0
fail() { echo "FAIL: $*" | tee -a "$OUT/checks.txt"; FAILS=$((FAILS + 1)); }
pass() { echo "ok: $*" | tee -a "$OUT/checks.txt"; }
pkill -x "$NAME"; pkill -x zenity
export IPLUG_TEST_DIALOG_DIR="$("$HERE/dialog-fixture.sh")"; D="$IPLUG_TEST_DIALOG_DIR"
"$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
APID=$!
sleep 5
read -r OX OY < <(xwininfo -root -tree | awk "/\"$NAME\": \(\"$NAME\" \"$NAME\"\)/{f=1;next} f&&/^ +0x/{split(\$NF,a,\"+\"); print a[2],a[3]; exit}")
echo "view origin $OX $OY, APP pid $APID"
wait_window() { for _ in $(seq 50); do w=$(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null | tail -1); [ -n "$w" ] && { sleep 0.8; xdotool windowactivate --sync "$w"; sleep 0.3; return 0; }; sleep 0.2; done; return 1; }
line() { sleep 1.5; grep -a '^IPLUG_TEST_DIALOG' "$OUT/stderr.txt" | sed -n "$1p"; }
check() { local got; got=$(line "$1"); if grep -q ' main=1 ' <<<"$got" && grep -Eq -- "$2" <<<"$got"; then pass "$got"; else fail "line $1 '$got' !~ /$2/ or off the main thread"; fi; }

xdotool mousemove $((OX + 889)) $((OY + 628)) click 1
wait_window "Save File" && { import -window root "$OUT/a01-save.png"; xdotool type --delay 50 "app пресет"; xdotool key Return; }
check 1 "save .*first='$D/app пресет\.fxp'"
xdotool mousemove $((OX + 889)) $((OY + 658)) click 1
wait_window "Dialog test" && { import -window root "$OUT/a02-msg.png"; xdotool key Escape; }
check 2 "msg-ync .*first='CANCEL'"
# the APP's window closed with a dialog open: the APP exits, the dialog goes with it
xdotool mousemove $((OX + 889)) $((OY + 628)) click 1
wait_window "Save File"
# a polite WM close: wmctrl's _NET_CLOSE_WINDOW (GNOME), else Alt+F4 (openbox). XDestroyWindow would kill the APP
# through a GDK X error instead, and Alt+F4 would close a modal dialog attached to it.
if command -v wmctrl >/dev/null; then wmctrl -F -c "$NAME"
else w=$(xdotool search --onlyvisible --name "^$NAME\$" | head -1); xdotool windowactivate --sync "$w"; sleep 0.3; xdotool key alt+F4; fi
sleep 3
kill -0 "$APID" 2>/dev/null && { fail "APP still running after its window closed"; kill "$APID"; } || pass "APP exited ($(tail -1 "$OUT/stderr.txt"))"
[ "$(pgrep -xc zenity)" = 0 ] && pass "dialog program gone with the APP" || fail "zenity left running: $(pgrep -a zenity)"
xdotool search --onlyvisible --name "^Save File\$" >/dev/null 2>&1 && fail "a Save File window is still showing (portal)" || pass "no Save File window left"
[ "$(grep -ac '^IPLUG_TEST_DIALOG' "$OUT/stderr.txt")" = 2 ] && pass "no handler after close" || fail "handler after close"
if [ "$FAILS" = 0 ]; then echo "RESULT: PASS" | tee "$OUT/RESULT.txt"; else echo "RESULT: FAIL ($FAILS)" | tee "$OUT/RESULT.txt"; fi
