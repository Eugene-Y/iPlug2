#!/usr/bin/env bash
# Exec a command in the GUI container with DISPLAY/dbus set, cwd = linux-research.
# Usage: gx.sh <name> <cmd...>    (gx.sh -d <name> ... runs detached)
set -euo pipefail
DET=""; [ "${1:-}" = "-d" ] && { DET="-d"; shift; }
NAME="$1"; shift
HERE="$(cd "$(dirname "$0")/.." && pwd)"
exec docker exec ${DET:--i} -w "$HERE" -e DISPLAY=:99 "$NAME" bash -c \
  'export DBUS_SESSION_BUS_ADDRESS=$(cat /tmp/dbus.addr); exec "$@"' _ "$@"
