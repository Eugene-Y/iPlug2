# quit Ardour, relaunch it with the t013 session (Gneiss' editor restores open), crop the logo; N times
N=${1:-3}; TAG=${2:-fixed}
for i in $(seq 1 $N); do
  sh ~/t013-quit.sh >/dev/null
  cd ~; setsid nohup ardour ~/t013-ardour/t013.ardour > ~/t013-ardour-gui.log 2>&1 < /dev/null &
  for k in $(seq 1 40); do sleep 1; xdotool search --onlyvisible --name "Gneiss:" >/dev/null 2>&1 && break; done
  sleep 4
  import -window root -crop 60x205+66+218 /tmp/logo-relaunch-$TAG-$i.png
  xdotool search --onlyvisible --name "Gneiss:" >/dev/null 2>&1 && echo "run $i: editor open" || echo "run $i: NO EDITOR"
done
timeout 8 xdpyinfo >/dev/null && echo X-OK
