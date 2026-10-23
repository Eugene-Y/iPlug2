# launch Ardour with the t013 session (Dummy backend), click through its start-up dialogs, open Gneiss' editor
pgrep -f '[/]usr/lib/ardour8/ardour-8' >/dev/null || { cd ~; setsid nohup ardour ~/t013-ardour/t013.ardour > ~/t013-ardour-gui.log 2>&1 < /dev/null & }
pgrep -x mutter-x11-fram >/dev/null || echo "WARNING: mutter-x11-frames is not running; decorated windows will not map"
for i in $(seq 1 40); do
  sleep 1
  xdotool search --onlyvisible --name "t013 - Ardour" >/dev/null 2>&1 && break
  w=$(xdotool search --onlyvisible --name "Audio/MIDI Setup" 2>/dev/null | head -1)
  [ -n "$w" ] && { eval $(xdotool getwindowgeometry --shell "$w"); xdotool mousemove $((X + WIDTH - 60)) $((Y + HEIGHT - 30)) click 1; sleep 2; }
  # "Engine I/O device has changed": a small dialog with an OK at its bottom right
  for d in $(xdotool search --onlyvisible --pid $(pgrep -f '[/]usr/lib/ardour8/ardour-8' | head -1) 2>/dev/null); do
    n=$(xdotool getwindowname "$d" 2>/dev/null)
    [ -z "$n" ] && { eval $(xdotool getwindowgeometry --shell "$d"); [ "$WIDTH" -lt 600 ] && [ "$HEIGHT" -lt 250 ] && xdotool mousemove $((X + WIDTH - 55)) $((Y + HEIGHT - 25)) click 1; }
  done
done
sleep 4
xdotool search --onlyvisible --name "Gneiss:" >/dev/null 2>&1 || {
  xdotool mousemove 260 400 click 1; sleep 1.5
  xdotool mousemove 120 268 click --repeat 2 --delay 80 1; sleep 4; }
xdotool search --onlyvisible --name . getwindowname %@ 2>/dev/null | sort -u
timeout 8 xdpyinfo >/dev/null && echo X-OK
