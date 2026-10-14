#!/usr/bin/env bash
# IGraphics popup menus (the plug-in's own IPopupMenuControl, and the stand-in used for platform menus) when the
# editor is partly off the screen: the menus stay inside the editor's bounds, so the screen edge may cut them.
# Usage: menu-edges.sh <out> "<FMT: IPlugControls>" <plugin>
set -uo pipefail
OUT="$1"; FX="$2"; PLUG="$3"; NAME="${FX#*: }"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"; R="$OUT/RESULT.txt"; : > "$R"
log() { echo "$*" | tee -a "$R"; }
"$HERE/reaper-up.sh" "$PLUG"; RPID=$(cat /tmp/reaper.pid)
rpr() { timeout 15 "$HERE/rpr.sh" "$1"; }
rpr "reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,\"$FX\",false,-1); reaper.TrackFX_Show(t,0,3)" >/dev/null
sleep 2.5
read -r SW SH < <(xdpyinfo | awk '/dimensions:/{split($2,a,"x"); print a[1], a[2]}')
fxwin() { for w in $(xdotool search --onlyvisible --name "$NAME.*Track 1" 2>/dev/null); do xprop -id "$w" WM_CLASS | grep -q '"REAPER"' && echo "$w"; done | tail -1; }
origin() { xwininfo -id "$(fxwin)" -tree | awk '/^ +0x[0-9a-f]+ /{ split($(NF-1),g,"[x+]"); split($NF,a,"+"); if (g[1]*g[2] > best) { best=g[1]*g[2]; r=a[2]" "a[3] } } END { print r }'; }
place() { # <view-x> <view-y> wanted for the editor's top-left; drags the FX window by its title bar (Mutter keeps
          # programmatic moves on screen, but lets the user drag a window partly off the right and bottom edges)
  read -r ox oy < <(origin)
  local cx cy; read -r cx cy < <(xwininfo -id "$(fxwin)" | awk '/Absolute upper-left X/{x=$NF} /Absolute upper-left Y/{y=$NF} END{print x, y}')
  local sx=$((cx + 300)) sy=$((cy - 16)) dx=$(($1 - ox)) dy=$(($2 - oy))
  xdotool mousemove "$sx" "$sy" mousedown 1
  for i in $(seq 1 20); do xdotool mousemove $((sx + dx * i / 20)) $((sy + dy * i / 20)); sleep 0.03; done
  xdotool mouseup 1; sleep 1.2
  read -r OX OY < <(origin); log "editor view at $OX,$OY (asked $1,$2), screen ${SW}x${SH}"
}
dismiss() { xdotool mousemove $((OX + 1010)) $((OY + 600)) click 1; sleep 0.6; }
dismiss_left() { xdotool mousemove $((OX + 60)) $((OY + 380)) click 1; sleep 0.6; }

# IVMenuButtonControl (view 828,284; the menu opens below/right of it) near the right edge of the screen
place $((SW - 828 - 50)) 120
xdotool mousemove $((OX + 828)) $((OY + 284)) click 1; sleep 1
import -window root "$OUT/e01-menu-right-edge.png"
log "right edge: button at screen x $((OX + 828)), the editor ends at $((OX + 1024)) (screen width $SW)"
dismiss_left
# ICaptionControl (view 321,99, dropdown below) near the bottom of the screen
place 200 $((SH - 99 - 40))
read -r _ ny < <(origin)
if [ $((OY + 99)) -lt "$SH" ]; then
  xdotool mousemove $((OX + 321)) $((OY + 99)) click 1; sleep 1
  import -window root "$OUT/e02-caption-bottom-edge.png"
  log "bottom edge: caption at screen y $((OY + 99)), editor ends at $((OY + 768)) (screen height $SH)"
  dismiss_left
else
  log "bottom edge: the window manager kept the caption control off screen (view y $OY)"
  import -window root "$OUT/e02-caption-bottom-edge.png"
fi
# the stand-in (platform) menus: toggle "OS Menu" at the normal position, then repeat the right-edge case
place 100 120
xdotool mousemove $((OX + 224)) $((OY + 720)) click 1; sleep 0.5
place $((SW - 828 - 50)) 120
xdotool mousemove $((OX + 828)) $((OY + 284)) click 1; sleep 1
import -window root "$OUT/e03-standin-menu-right-edge.png"
dismiss_left
ps -o stat= -p "$RPID" >/dev/null && log "REAPER alive" || log "REAPER DEAD"
pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null
