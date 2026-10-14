#!/usr/bin/env bash
# Final P3b verification (reviewer), from the HOST: ASan repros, full step scripts, APP, EDIT churn, unit test.
# Regenerates evidence/p3b/{reaper-controls-*,app-controls,churn-edit-controls-*,input-test.txt}.
# Usage: final-verify.sh [build=build/final-p3b] [container=p3brev]
set -u
cd "$(dirname "$0")/../.."
B="${1:-build/final-p3b}"; C="${2:-p3brev}"; A="$B-asan/out/IPlugControls"; F=evidence/p3b-review/final; P=evidence/p3b
mkdir -p "$F"
gx() { scripts/gx.sh "$C" "$@"; }
sheet() { # <dir> <glob>
  local o; o=$(grep -m1 -oE 'origin [0-9]+,[0-9]+' "$1/xact.log" "$1/run.log" 2>/dev/null | head -1 | grep -oE '[0-9]+,[0-9]+')
  gx bash -c "cd '$1' && montage -label '%t' $2 -crop 1024x768+${o%,*}+${o#*,} +repage -tile 4x -geometry 512x384+4+4 sheet.png" >/dev/null 2>&1
}
echo "== ASan repros"
gx timeout 300 evidence/p3b-review/app-s2.sh "$A" "$F/app-s2-asan" 2>&1 | tail -2
gx timeout 300 evidence/p3b-review/app-s3.sh "$A" "$F/app-s3-asan" 2>&1 | tail -2
GUI=$C scripts/run-gui-test.sh "$A" "$F/app-s1-asan" evidence/p3b-review/app-standins.txt "$C" > "$F/app-s1.log" 2>&1
echo "S1: dead=$(grep -c DEAD "$F/app-s1-asan/xact.log") asan=$(grep -c AddressSanitizer "$F/app-s1-asan/stderr.txt")"
for f in clap vst3; do
  rm -rf "$P/reaper-controls-$f"
  GUI=$C scripts/run-gui-test.sh "$B/out/IPlugControls.$f" "$P/reaper-controls-$f" scripts/steps/iplugcontrols-p3b.txt "$C" > /dev/null 2>&1
  sheet "$P/reaper-controls-$f" 'c*.png'
  echo "$f: $(head -1 "$P/reaper-controls-$f/RESULT.txt")"
done
rm -rf "$P/app-controls"
GUI=$C scripts/run-gui-test.sh "$B/out/IPlugControls" "$P/app-controls" scripts/steps/iplugcontrols-app-p3b.txt "$C" > /dev/null 2>&1
sheet "$P/app-controls" 'a*.png'
echo "app: fails=$(grep -c FAIL "$P/app-controls/xact.log") dead=$(grep -c DEAD "$P/app-controls/xact.log")"
for f in clap vst3; do
  rm -rf "$P/churn-edit-controls-$f"
  FMT=$(echo $f | tr a-z A-Z)
  docker exec -w "$PWD" -e DISPLAY=:99 -e EDIT=1 "$C" bash -c "export DBUS_SESSION_BUS_ADDRESS=\$(cat /tmp/dbus.addr); timeout 1800 scripts/editor-churn-test.sh $P/churn-edit-controls-$f '$FMT: IPlugControls' $B/out/IPlugControls.$f 100" > /dev/null 2>&1
  echo "churn $f: $(tail -1 "$P/churn-edit-controls-$f/churn.txt")"
done
gx timeout 300 bash -c "export LSAN_OPTIONS=suppressions=\$PWD/../Tests/LinuxInput/lsan.supp; echo '# us layout'; setxkbmap us; $B-input/iplug-linux-input-test; echo \"exit \$?\"; echo '# us,ru layout'; setxkbmap -layout us,ru; $B-input/iplug-linux-input-test; echo \"exit \$?\"; setxkbmap us" > "$P/input-test.txt" 2>&1
echo "input: $(grep -c '^exit 0' "$P/input-test.txt")/2 exit 0"
echo "== done"
