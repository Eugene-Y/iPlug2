# quit Ardour from its main window (Ctrl+Q; "Just quit" if it asks), wait until it is gone
xdotool mousemove 1150 49 click 1; sleep 0.5; xdotool key ctrl+q; sleep 2
xdotool search --onlyvisible --name "Unsaved Session" >/dev/null 2>&1 && xdotool mousemove 900 196 click 1
for i in $(seq 1 15); do sleep 1; pgrep -f '[/]usr/lib/ardour8/ardour-8' >/dev/null || break; done
pgrep -f '[/]usr/lib/ardour8/ardour-8' >/dev/null && echo ARDOUR-STILL-RUNNING || echo ardour-gone
