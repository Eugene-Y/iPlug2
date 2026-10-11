#!/usr/bin/env bash
# Editor open/close churn in REAPER: fds, threads, X clients and RSS before and after. Runs INSIDE the GUI container.
# Usage: editor-churn-test.sh <outdir> "<FMT: Name>" <plugin> [cycles=100]
# Two instances on two tracks; each cycle toggles both editors. Most cycles leave time for frames and X events
# while open (timers and fd watches live); every 5th cycle closes again within the same Lua call, before any
# timer of the new window has fired.
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; N="${4:-100}"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"; R="$OUT/churn.txt"; : > "$R"
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
rpr() { "$HERE/rpr.sh" "$1"; }
census() { # <label>
  local fds thr rss
  fds=$(ls /proc/$RPID/fd | wc -l); thr=$(ls /proc/$RPID/task | wc -l)
  rss=$(awk '/VmRSS/{print $2}' /proc/$RPID/status);
  local socks; socks=$(ls -l /proc/$RPID/fd | grep -c socket)
  echo "$1 fds=$fds sockets=$socks threads=$thr rss_kb=$rss" | tee -a "$R"
}
rpr "for i=0,1 do reaper.InsertTrackAtIndex(i,true); reaper.TrackFX_AddByName(reaper.GetTrack(0,i),\"$FX\",false,-1) end" >/dev/null
sleep 2
census "closed-before"
rpr "for i=0,1 do reaper.TrackFX_Show(reaper.GetTrack(0,i),0,3) end" >/dev/null; sleep 2
census "open-before"
rpr "for i=0,1 do reaper.TrackFX_Show(reaper.GetTrack(0,i),0,2) end" >/dev/null; sleep 1
for c in $(seq 1 "$N"); do
  if [ $((c % 5)) = 0 ]; then
    out=$(rpr "for i=0,1 do local t=reaper.GetTrack(0,i); reaper.TrackFX_Show(t,0,3); reaper.TrackFX_Show(t,0,2) end; out('ok')")
  else
    out=$(rpr "for i=0,1 do reaper.TrackFX_Show(reaper.GetTrack(0,i),0,3) end; out('ok')"); sleep 0.25
    xdotool mousemove $((300 + c % 200)) 400; sleep 0.1
    out=$(rpr "for i=0,1 do reaper.TrackFX_Show(reaper.GetTrack(0,i),0,2) end; out('ok')")
  fi
  grep -q ok <<<"$out" || { echo "cycle $c: $out" | tee -a "$R"; }
  st=$(ps -o stat= -p "$RPID" 2>/dev/null); [[ -n "$st" && "$st" != Z* ]] || { echo "REAPER DEAD at cycle $c" | tee -a "$R"; exit 1; }
  [ $((c % 25)) = 0 ] && census "after-$c-closed"
done
sleep 1
census "closed-after"
rpr "for i=0,1 do reaper.TrackFX_Show(reaper.GetTrack(0,i),0,3) end" >/dev/null; sleep 2
census "open-after"
import -window root "$OUT/open-after.png"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
grep -E "^(closed|open)-(before|after)" "$R" | awk '{for(i=2;i<=NF;i++){split($i,a,"="); v[$1,a[1]]=a[2]}} END{
  split("fds sockets threads",k," "); bad=0
  for (j in k) { if (v["closed-after",k[j]] > v["closed-before",k[j]] || v["open-after",k[j]] > v["open-before",k[j]]) { print "GREW " k[j]; bad=1 } }
  print (bad ? "RESULT: FAIL" : "RESULT: PASS") }' | tee -a "$R"
