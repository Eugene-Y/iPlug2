#!/usr/bin/env bash
# Reviewer probe (T-008): the P3b script's Ctrl+C step repeated. In one REAPER session, N times: double-click the
# IVNumberBox (xdotool click --repeat 2 --delay 80, same-ms press/release, as xact's dclick), wait, Ctrl+C, Escape.
# The edit must hold the focus: the clipboard gets the value, Escape cancels (the FX window stays).
# Runs inside the GUI container, cwd linux-research. Usage: focus-copy.sh <out> "<FMT: IPlugControls>" <plugin> [N=30] [wait=1]
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; N="${4:-30}"; WAIT="${5:-1}"; NAME="${FX#*: }"
HERE="$PWD/scripts"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
gcc -O1 -include stdlib.h -o /tmp/focuswatch "$HERE/probe/focuswatch.c" -lX11 2>/dev/null || { log "cannot build focuswatch"; exit 1; }
"$HERE/reaper-up.sh" "$PLUG"
rpr() { timeout 15 "$HERE/rpr.sh" "$1"; }
show() { rpr "local t=reaper.GetTrack(0,0); reaper.TrackFX_Show(t,0,3)" >/dev/null; sleep 2; }
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1)" >/dev/null
show
fxwin() { for w in $(xdotool search --onlyvisible --name "$NAME.*Track 1" 2>/dev/null); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1; }
origin() {
  local w; w=$(fxwin); [ -n "$w" ] || return 1
  xwininfo -id "$w" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }'
}
gain() { rpr "local ok,s=reaper.TrackFX_GetFormattedParamValue(reaper.GetTrack(0,0),0,0,''); out(s)"; }
ok=0
for i in $(seq "$N"); do
  read -r OX OY < <(origin) || { log "trial $i: FX window missing, reopening"; show; read -r OX OY < <(origin); }
  printf 'none' | xclip -selection clipboard
  before=$(gain)
  timeout $((WAIT + 3)) /tmp/focuswatch $((WAIT + 2)) > "$OUT/focus-$i.txt" 2>&1 &
  FW=$!
  xdotool mousemove $((OX + 941)) $((OY + 437)) click --repeat 2 --delay 80 1
  sleep "$WAIT"
  xdotool key ctrl+c; xdotool key Escape; sleep 0.7
  wait "$FW"
  clip=$(timeout 2 xclip -o -selection clipboard 2>/dev/null)
  after=$(gain); w=$(fxwin)
  if [ -n "$w" ] && [ "$after" = "$before" ] && [ "${clip%%.*}" = "${before%%.*}" ]; then ok=$((ok + 1)); res=ok
  else res="FAIL (window ${w:-gone}, gain '$before' -> '$after', clipboard '$clip')"; fi
  log "trial $i: $res; focus trail: $(awk '{print $3}' "$OUT/focus-$i.txt" | tr '\n' ' ')"
  [ -n "$w" ] || show
  sleep 0.3
done
log "RESULT: $ok/$N"
grep -E 'focus|Focus' /tmp/reaper.log > "$OUT/editor-focus-trace.txt" 2>/dev/null
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
true
