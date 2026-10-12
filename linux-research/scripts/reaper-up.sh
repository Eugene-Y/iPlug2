#!/usr/bin/env bash
# Start REAPER (aarch64/x86_64 Linux build in /opt/REAPER) with a fresh profile and the given plugins
# installed into that profile's ~/.clap / ~/.vst3. Runs INSIDE the GUI container (gx.sh).
# Usage: reaper-up.sh <plugin.clap|plugin.vst3> ...
# Then drive it with rpr.sh '<lua>' ; screenshots with shot.sh.
set -euo pipefail
export HOME=/tmp/rhome
pkill -x reaper 2>/dev/null || true; pkill -f '^/opt/REAPER/reaper -nonewinst' 2>/dev/null || true; sleep 0.5
rm -rf "$HOME"; mkdir -p "$HOME/.clap" "$HOME/.vst3" "$HOME/.config/REAPER"
for p in "$@"; do
  case "$p" in
    *.clap) cp -a "$p" "$HOME/.clap/"
            # flat .clap layouts (PR #1336) look for resources/ next to the .clap, as their Deploy step does
            [ -d "$(dirname "$p")/resources" ] && cp -a "$(dirname "$p")/resources" "$HOME/.clap/"
            # per-plugin <Name>.resources/ next to the .clap (stillwell layout)
            [ -d "${p%.clap}.resources" ] && cp -a "${p%.clap}.resources" "$HOME/.clap/";;
    *.vst3) cp -a "$p" "$HOME/.vst3/";;
  esac
done
# JACK dummy backend so REAPER (and the plugins) run a real audio thread without a sound card.
pgrep -x jackd >/dev/null || { jackd --no-realtime -d dummy -r 48000 -p 256 >/tmp/jackd.log 2>&1 & sleep 1.5; }
cat > "$HOME/.config/REAPER/reaper.ini" <<'EOF'
[REAPER]
linux_audio_mode=2
splashscreen=0
EOF
nohup /opt/REAPER/reaper -nosplash -new > /tmp/reaper.log 2>&1 &
echo $! > /tmp/reaper.pid
for i in $(seq 100); do xdotool search --name '^REAPER v' >/dev/null 2>&1 && break; sleep 0.2; done
sleep 2
# dismiss the evaluation "About" nag. `xdotool windowclose` is XDestroyWindow on REAPER's window: on GNOME/X11 it
# kills mutter-x11-frames, after which every new window stays Iconic (unmapped). A polite WM close where we can;
# the nag ignores it until its countdown has run out, hence the retries.
for i in $(seq 10); do
  ws=$(xdotool search --onlyvisible --name '^About REAPER' 2>/dev/null) || break
  for w in $ws; do
    if command -v wmctrl >/dev/null; then wmctrl -i -c "$w"; else xdotool windowclose "$w" 2>/dev/null || xdotool key --window "$w" Escape; fi
  done
  sleep 1
done
# a project reset right after startup can swallow early script edits: wait until REAPER answers, then settle
"$(dirname "$0")/rpr.sh" 'out("ready")' >/dev/null; sleep 2
echo "REAPER up: $(xdotool search --name '^REAPER v' getwindowname %@ 2>/dev/null | head -1)"
