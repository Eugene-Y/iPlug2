#!/usr/bin/env bash
# Start REAPER (aarch64/x86_64 Linux build in /opt/REAPER) with a fresh profile and the given plugins
# installed into that profile's ~/.clap / ~/.vst3. Runs INSIDE the GUI container (gx.sh).
# Usage: reaper-up.sh <plugin.clap|plugin.vst3|reaper_<ext>.so> ...
#   An extension goes into the profile's UserPlugins/, with a <ext>/ folder beside it if there is one (web UI files).
# Env: REAPER_EXIT_FILE=<path>: REAPER's exit status is written there when it ends.
#      REAPER_UI_SCALE=1.5 sets SWELL's ui_scale (REAPER's own HiDPI setting, which it passes on to CLAP set_scale
#      and VST3 setContentScaleFactor).
#      REAPER_WRAPPER="stdbuf -oL -eL" runs REAPER through that command (line-buffered plug-in printf output).
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
    */reaper_*.so)
            mkdir -p "$HOME/.config/REAPER/UserPlugins"; cp -a "$p" "$HOME/.config/REAPER/UserPlugins/"
            ext=$(basename "$p" .so); ext="${ext#reaper_}"
            [ -d "$(dirname "$p")/$ext" ] && cp -a "$(dirname "$p")/$ext" "$HOME/.config/REAPER/UserPlugins/";;
  esac
done
# Unused while reaper.ini selects Dummy Audio (linux_audio_mode=2), which already runs an audio thread without a
# sound card; switching to JACK (mode 0) needs this server.
pgrep -x jackd >/dev/null || { jackd --no-realtime -d dummy -r 48000 -p 256 >/tmp/jackd.log 2>&1 & sleep 1.5; }
# verchk=0: no update check, whose notice (a modal since 7.83 came out) blocks the Lua calls of rpr.sh.
cat > "$HOME/.config/REAPER/reaper.ini" <<'EOF'
[REAPER]
linux_audio_mode=2
splashscreen=0
verchk=0
EOF
[ -n "${REAPER_UI_SCALE:-}" ] && printf '[.swell]\nui_scale=%s\n' "$REAPER_UI_SCALE" >> "$HOME/.config/REAPER/reaper.ini"
if [ -n "${REAPER_EXIT_FILE:-}" ]; then
  # A parent that outlives REAPER records how it ended (exit status 128+N = killed by signal N).
  rm -f "$REAPER_EXIT_FILE"
  ( ${REAPER_WRAPPER:-} /opt/REAPER/reaper -nosplash -new > /tmp/reaper.log 2>&1; echo $? > "$REAPER_EXIT_FILE" ) &
  for i in $(seq 50); do pgrep -n -x reaper > /tmp/reaper.pid && break; sleep 0.1; done
else
  nohup ${REAPER_WRAPPER:-} /opt/REAPER/reaper -nosplash -new > /tmp/reaper.log 2>&1 &
  echo $! > /tmp/reaper.pid
fi
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
