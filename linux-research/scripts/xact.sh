#!/usr/bin/env bash
# Tiny xdotool DSL for GUI tests. Runs INSIDE the GUI container. Reads steps from stdin, one per line.
# Coordinates are relative to ORIGIN (set with `origin X Y` or `origin-win <window-name-regex>`).
#   origin X Y | origin-win REGEX      set coordinate origin (plugin view top-left on screen)
#   click X Y [button] | dclick X Y | rclick X Y
#   drag X1 Y1 X2 Y2                   press, move in 10 steps, release
#   key KEYS... | type TEXT...
#   wait SECS
#   shot NAME                          screenshot -> $OUT/NAME.png
#   alive PROC|PID                     prints ALIVE/DEAD for a process name or pid
#   windows                            list named top-level windows
#   expect-change A B X Y W H          shots A and B must differ inside the view rect; else a line in $OUT/xact-fails.txt
#   # comment
# Usage: OUT=<dir> xact.sh < steps.txt
set -uo pipefail
OUT="${OUT:-/tmp/xact}"; mkdir -p "$OUT"
OX=0; OY=0
log() { echo "[xact] $*"; }
while read -r cmd a b c d rest; do
  case "$cmd" in
    ''|\#*) continue;;
    origin) OX=$a; OY=$b; log "origin $OX,$OY";;
    origin-win)
      w=$(xdotool search --onlyvisible --name "$a" | tail -1)
      eval "$(xdotool getwindowgeometry --shell "$w")"; OX=$X; OY=$Y; log "origin-win '$a' -> $OX,$OY (${WIDTH}x${HEIGHT})";;
    click) xdotool mousemove $((OX+a)) $((OY+b)) click "${c:-1}"; log "click $a $b";;
    rclick) xdotool mousemove $((OX+a)) $((OY+b)) click 3; log "rclick $a $b";;
    dclick) xdotool mousemove $((OX+a)) $((OY+b)) click --repeat 2 --delay 80 1; log "dclick $a $b";;
    drag)
      xdotool mousemove $((OX+a)) $((OY+b)) mousedown 1
      for i in $(seq 1 10); do xdotool mousemove $((OX+a+(c-a)*i/10)) $((OY+b+(d-b)*i/10)); sleep 0.03; done
      xdotool mouseup 1; log "drag $a,$b -> $c,$d";;
    key) xdotool key $a $b $c $d $rest; log "key $a $b $c $d $rest";;
    type) xdotool type --delay 40 "$a${b:+ $b}${c:+ $c}${d:+ $d}${rest:+ $rest}"; log "type $a $b";;
    wait) sleep "$a";;
    shot) import -window root "$OUT/$a.png"; log "shot $a";;
    alive)
      if [[ "$a" =~ ^[0-9]+$ ]]; then st=$(ps -o stat= -p "$a" 2>/dev/null); [[ -n "$st" && "$st" != Z* ]] && log "ALIVE pid $a" || log "DEAD pid $a"
      elif pgrep -x "$a" >/dev/null; then log "ALIVE $a"; else log "DEAD $a"; fi;;
    expect-change)
      read -r w h _ <<<"$rest"
      crop="${w}x${h}+$((OX+c))+$((OY+d))"
      ae=$(compare -metric AE <(convert "$OUT/$a.png" -crop "$crop" +repage png:-) <(convert "$OUT/$b.png" -crop "$crop" +repage png:-) null: 2>&1)
      if [ "${ae%% *}" = 0 ]; then log "UNCHANGED $a -> $b in view rect $c,$d ${w}x${h}"; echo "expect-change: $a -> $b unchanged in view rect $c,$d ${w}x${h}" >> "$OUT/xact-fails.txt"
      else log "changed $a -> $b in view rect $c,$d ${w}x${h} ($ae px)"; fi;;
    windows) xdotool search --onlyvisible --name '.' getwindowname %@ 2>/dev/null | sort -u | sed 's/^/[xact]   win: /';;
    *) log "unknown step: $cmd";;
  esac
  sleep 0.3
done
