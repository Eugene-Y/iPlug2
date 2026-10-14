#!/usr/bin/env bash
# kdialog path through the standalone APP (IPLUG_LINUX_DIALOGS=kdialog). Runs INSIDE a GUI container that has
# kdialog (not in the image: `apt-get install kdialog` in a throwaway container). Screenshots of every dialog,
# handler lines in <out>/lines.txt. Usage: dialog-kdialog-probe.sh <outdir> <app binary>
set -uo pipefail
OUT="$1"; APP="$2"; NAME=$(basename "$APP")
HERE="$(cd "$(dirname "$0")" && pwd)"
export LC_ALL=C.UTF-8 IPLUG_LINUX_DIALOGS=kdialog
mkdir -p "$OUT"
pkill -x "$NAME"
export IPLUG_TEST_DIALOG_DIR="$("$HERE/dialog-fixture.sh")"
"$APP" --no-io > "$OUT/stderr.txt" 2>&1 &
sleep 5
read -r OX OY < <(xwininfo -root -tree | awk "/\"$NAME\": \(\"$NAME\" \"$NAME\"\)/{f=1;next} f&&/^ +0x/{split(\$NF,a,\"+\"); print a[2],a[3]; exit}")
step() { # <button x y> <shot> <keys...>
  local bx=$1 by=$2 shot=$3; shift 3
  xdotool mousemove $((OX + bx)) $((OY + by)) click 1; sleep 4
  w=$(xdotool search --onlyvisible --pid "$(pgrep -nx kdialog)" 2>/dev/null | tail -1)
  [ -n "$w" ] && xdotool windowactivate --sync "$w"; sleep 0.5
  import -window root "$OUT/$shot.png"
  for k in "$@"; do case "$k" in type:*) xdotool type --delay 60 "${k#type:}";; *) xdotool key "$k";; esac; sleep 0.5; done
  sleep 2
}
step 889 628 k01-save "type:kd пресет" Return
step 759 628 k02-open "type:старый пресет.fxp" Return
step 759 658 k03-dir Return
step 889 658 k04-ync Escape
step 759 688 k05-ok Return
step 889 688 k06-okc Return
step 759 718 k07-color Return
grep -a '^IPLUG_TEST_DIALOG' "$OUT/stderr.txt" | tee "$OUT/lines.txt"
pkill -x "$NAME"; pkill -x kdialog
