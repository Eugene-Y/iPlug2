# Start the Renoise demo from its folder (stderr = iPlug's trace) unless it runs, click "Ok" on its start-up
# notices (the RT-thread warning: a DIALOG named "Renoise", Ok at its bottom centre), list its windows.
# Usage: launch.sh [log] [song.xrns]
RN=~/hosts/Renoise_3_5_4_Demo_Linux_arm64
LOG=${1:-$HOME/t013-renoise.log}
pgrep -x renoise >/dev/null || { cd $RN; setsid nohup ./renoise ${2:+"$2"} > "$LOG" 2>&1 < /dev/null & }
for i in $(seq 1 ${WAIT:-40}); do
  sleep 1
  for w in $(xdotool search --onlyvisible --class renoise 2>/dev/null); do
    [ "$(xdotool getwindowname $w 2>/dev/null)" = Renoise ] || continue
    xprop -id $w _NET_WM_WINDOW_TYPE 2>/dev/null | grep -q DIALOG || continue
    eval $(xdotool getwindowgeometry --shell $w)
    xdotool mousemove $((X + WIDTH / 2)) $((Y + HEIGHT - 20)) mousedown 1; sleep 0.08; xdotool mouseup 1
    echo "dismissed a start-up notice"; sleep 1
  done
  xdotool search --onlyvisible --name "^Renoise \(arm64\)" >/dev/null 2>&1 && break
done
sleep 2
for w in $(xdotool search --onlyvisible --class renoise 2>/dev/null); do
  echo "$w $(xdotool getwindowname $w 2>/dev/null) $(xdotool getwindowgeometry $w 2>/dev/null | sed -n 2,3p | tr '\n' ' ')"
done
timeout 8 xdpyinfo >/dev/null && echo X-OK
