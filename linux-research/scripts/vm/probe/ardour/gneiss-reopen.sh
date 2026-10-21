# close and reopen Gneiss' editor in Ardour N times, cropping the logo after each open
N=${1:-6}; TAG=${2:-fixed}
for i in $(seq 1 $N); do
  xdotool search --onlyvisible --name "Gneiss:" >/dev/null 2>&1 && xdotool mousemove 988 170 click 1
  sleep 1.5
  xdotool mousemove 260 400 click 1; sleep 1
  xdotool mousemove 120 268 click --repeat 2 --delay 80 1; sleep 3
  import -window root -crop 60x205+66+218 /tmp/logo-$TAG-$i.png
done
timeout 8 xdpyinfo >/dev/null && echo X-OK
