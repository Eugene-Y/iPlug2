#!/usr/bin/env bash
# Start a detached GUI test container: Xvfb :99 + openbox + dbus session.
# Usage: gui-up.sh [name=iplug-gui] ; then: gx.sh <name> <cmd...>  ; gui-down: docker rm -f <name>
# Env: XVFB_SCREEN (default 1600x1000x24), e.g. 2600x1900x24 for a 1024x768 editor at scale 2.
set -euo pipefail
NAME="${1:-iplug-gui}"
SCREEN="${XVFB_SCREEN:-1600x1000x24}"
HERE="$(cd "$(dirname "$0")" && pwd)"
docker rm -f "$NAME" >/dev/null 2>&1 || true
"$HERE/dock.sh" -d -n "$NAME" -- bash -c '
  Xvfb :99 -screen 0 '"$SCREEN"' -nolisten tcp >/tmp/xvfb.log 2>&1 &
  for i in $(seq 50); do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 0.1; done
  export DISPLAY=:99
  eval "$(dbus-launch --sh-syntax)"; echo "$DBUS_SESSION_BUS_ADDRESS" > /tmp/dbus.addr
  openbox >/tmp/openbox.log 2>&1 &
  xsetroot -solid "#303040" 2>/dev/null || true
  sleep infinity'
for i in $(seq 50); do docker exec "$NAME" test -f /tmp/dbus.addr 2>/dev/null && break; sleep 0.2; done
echo "GUI container $NAME up (DISPLAY=:99)"
