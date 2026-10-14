#!/usr/bin/env bash
# Editor open/close churn in REAPER with a dialog open at every close (IPlugControls -DIPLUG_TEST_DIALOGS).
# Runs INSIDE the GUI container. Usage: dialog-churn-test.sh <outdir> "<FMT: Name>" <plugin> [cycles=50]
# Each cycle opens the editor, opens a dialog (save, folder, message box, colour in turn) and closes the editor while
# it shows; every 5th cycle closes right after the click, before the dialog's window can map. After each close the
# dialog program must be gone and no handler may have run; fds, sockets, threads and REAPER's child processes
# must be the same before and after.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; N="${4:-50}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"; R="$OUT/churn.txt"; : > "$R"
read -r _ OX OY < <("$HERE/dialog-up.sh" "$FX" "$PLUG")
RPID=$(cat /tmp/reaper.pid)
rpr() { "$HERE/rpr.sh" "$1"; }
census() {
  local fds socks thr kids
  fds=$(ls /proc/$RPID/fd | wc -l); socks=$(ls -l /proc/$RPID/fd | grep -c socket); thr=$(ls /proc/$RPID/task | wc -l)
  kids=$(ps -o pid= --ppid "$RPID" | wc -l)
  echo "$1 fds=$fds sockets=$socks threads=$thr children=$kids helpers=$(pgrep -xc 'zenity|kdialog')" | tee -a "$R"
}
show() { rpr "reaper.TrackFX_Show(reaper.GetTrack(0,0),0,$1); out('ok')" >/dev/null; }
BUTTONS=("889 628" "759 658" "889 658" "759 718") # t-save t-dir t-ync t-color
TITLES=("Save File" "Choose a Directory" "Dialog test" "Dialog test colour")
before=$(grep -ac '^IPLUG_TEST_DIALOG' /tmp/reaper.log)
show 2; sleep 1
census closed-before
show 3; sleep 2; census open-before
bad=0
for c in $(seq 1 "$N"); do
  [ $c = 1 ] || { show 3; sleep 1.2; }
  k=$((c % 4)); read -r bx by <<<"${BUTTONS[$k]}"
  xdotool mousemove $((OX + bx)) $((OY + by)) click 1
  if [ $((c % 5)) = 0 ]; then
    show 2
  else
    for _ in $(seq 40); do xdotool search --onlyvisible --name "^${TITLES[$k]}\$" >/dev/null 2>&1 && break; sleep 0.1; done
    xdotool search --onlyvisible --name "^${TITLES[$k]}\$" >/dev/null 2>&1 || { echo "cycle $c: '${TITLES[$k]}' never showed" | tee -a "$R"; bad=1; }
    show 2
  fi
  sleep 0.8
  left=$(pgrep -xc 'zenity|kdialog')
  [ "$left" = 0 ] || { echo "cycle $c: $left dialog programs still running after close" | tee -a "$R"; bad=1; }
  st=$(ps -o stat= -p "$RPID" 2>/dev/null); [[ -n "$st" && "$st" != Z* ]] || { echo "REAPER DEAD at cycle $c" | tee -a "$R"; exit 1; }
  [ $((c % 10)) = 0 ] && census "after-$c-closed"
done
sleep 1
census closed-after
show 3; sleep 2; census open-after
import -window root "$OUT/open-after.png"
handlers=$(( $(grep -ac '^IPLUG_TEST_DIALOG' /tmp/reaper.log) - before ))
echo "handler calls during the churn: $handlers" | tee -a "$R"
[ "$handlers" = 0 ] || bad=1
zombies=$(ps -o stat= --ppid "$RPID" | grep -c '^Z')
echo "zombie children: $zombies" | tee -a "$R"
[ "$zombies" = 0 ] || bad=1
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
grep -E "^(closed|open)-(before|after)" "$R" | awk -v bad=$bad '{for(i=2;i<=NF;i++){split($i,a,"="); v[$1,a[1]]=a[2]}} END{
  split("fds sockets threads children helpers",k," ")
  for (j in k) { if (v["closed-after",k[j]] > v["closed-before",k[j]] || v["open-after",k[j]] > v["open-before",k[j]]) { print "GREW " k[j]; bad=1 } }
  print (bad ? "RESULT: FAIL" : "RESULT: PASS") }' | tee -a "$R"
