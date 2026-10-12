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
#   expect-same A B X Y W H            shots A and B must be identical inside the view rect
#   expect-param TRACK PARAM REGEX     REAPER: formatted value of FX 0's PARAM on track TRACK (0-based) matches REGEX
#   expect-pointer X Y                 the pointer is at view X,Y (within 2 px)
#   set-clipboard TEXT                 CLIPBOARD := TEXT (xclip)
#   expect-clipboard TEXT              CLIPBOARD reads TEXT, both with xclip and with xsel
#   move X Y                           move the pointer (hover)
#   lua-expect REGEX LUA...            REAPER: what LUA writes with out() matches REGEX
#   expect-window REGEX                a visible window named REGEX exists
#   expect-cursor NAME|blank           the pointer's cursor has this (theme) name, or no opaque pixel
#   drag-check X1 Y1 X2 Y2 CHECK...    like drag, and runs the step CHECK... before the release
#   # comment
# Usage: OUT=<dir> xact.sh < steps.txt
set -uo pipefail
OUT="${OUT:-/tmp/xact}"; mkdir -p "$OUT"
read -r OX OY <<<"${XACT_ORIGIN:-0 0}"
HERE="$(cd "$(dirname "$0")" && pwd)"
log() { echo "[xact] $*"; }
xfail() { log "FAIL $*"; echo "$*" >> "$OUT/xact-fails.txt"; }
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
    drag|drag-check)
      xdotool mousemove $((OX+a)) $((OY+b)) mousedown 1
      for i in $(seq 1 10); do xdotool mousemove $((OX+a+(c-a)*i/10)) $((OY+b+(d-b)*i/10)); sleep 0.03; done
      [ "$cmd" = drag-check ] && echo "$rest" | OX=$OX OY=$OY OUT="$OUT" XACT_ORIGIN="$OX $OY" "$0"
      xdotool mouseup 1; log "drag $a,$b -> $c,$d";;
    expect-cursor)
      [ -x /tmp/cursor-probe ] || gcc -O1 -o /tmp/cursor-probe "$HERE/cursor-probe.c" -lX11 -lXfixes
      read -r cname csize copaque < <(/tmp/cursor-probe)
      if [ "$a" = blank ] && [ "$copaque" = 0 ]; then log "cursor blank ($csize)"
      elif [ "$a" = "$cname" ]; then log "cursor $cname ($csize)"
      else xfail "expect-cursor: '$cname' $csize, $copaque opaque px; expected $a"; fi;;
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
      if [ "${ae%% *}" = 0 ]; then xfail "expect-change: $a -> $b unchanged in view rect $c,$d ${w}x${h}"
      else log "changed $a -> $b in view rect $c,$d ${w}x${h} ($ae px)"; fi;;
    expect-same)
      read -r w h _ <<<"$rest"
      crop="${w}x${h}+$((OX+c))+$((OY+d))"
      ae=$(compare -metric AE <(convert "$OUT/$a.png" -crop "$crop" +repage png:-) <(convert "$OUT/$b.png" -crop "$crop" +repage png:-) null: 2>&1)
      if [ "${ae%% *}" = 0 ]; then log "same $a -> $b in view rect $c,$d ${w}x${h}"
      else xfail "expect-same: $a -> $b differ in view rect $c,$d ${w}x${h} ($ae px)"; fi;;
    expect-param)
      v=$("$HERE/rpr.sh" "local t=reaper.GetTrack(0,$a); local ok,s=reaper.TrackFX_GetFormattedParamValue(t,0,$b,''); out(s)")
      if grep -Eq -- "$c" <<<"$v"; then log "param $a/$b = '$v' (matches $c)"; else xfail "expect-param: track $a param $b = '$v', expected /$c/"; fi;;
    expect-pointer)
      eval "$(xdotool getmouselocation --shell)"
      if [ $(( (X-OX-a)*(X-OX-a) + (Y-OY-b)*(Y-OY-b) )) -le 8 ]; then log "pointer at view $((X-OX)),$((Y-OY))"
      else xfail "expect-pointer: pointer at view $((X-OX)),$((Y-OY)), expected $a,$b"; fi;;
    set-clipboard) printf '%s' "$a${b:+ $b}${c:+ $c}${d:+ $d}${rest:+ $rest}" | xclip -selection clipboard; log "set-clipboard";;
    expect-clipboard)
      want="$a${b:+ $b}${c:+ $c}${d:+ $d}${rest:+ $rest}"
      got1=$(timeout 3 xclip -o -selection clipboard 2>&1); got2=$(timeout 3 xsel -b -o 2>&1)
      if [ "$got1" = "$want" ] && [ "$got2" = "$want" ]; then log "clipboard '$want' (xclip, xsel)"
      else xfail "expect-clipboard: xclip '$got1', xsel '$got2', expected '$want'"; fi;;
    move) xdotool mousemove $((OX+a)) $((OY+b)); log "move $a $b";;
    lua-expect)
      v=$("$HERE/rpr.sh" "$b${c:+ $c}${d:+ $d}${rest:+ $rest}")
      if grep -Eq -- "$a" <<<"$v"; then log "lua '$v' (matches $a)"; else xfail "lua-expect: '$v', expected /$a/ from: $b $c $d $rest"; fi;;
    expect-window)
      if xdotool search --onlyvisible --name "$a" >/dev/null 2>&1; then log "window /$a/ exists"; else xfail "expect-window: no visible window /$a/"; fi;;
    windows) xdotool search --onlyvisible --name '.' getwindowname %@ 2>/dev/null | sort -u | sed 's/^/[xact]   win: /';;
    *) log "unknown step: $cmd";;
  esac
  sleep 0.3
done
