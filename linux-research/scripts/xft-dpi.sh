#!/usr/bin/env bash
# Set or remove Xft.dpi in the X resources (RESOURCE_MANAGER on the root window), as GNOME/KDE do, without xrdb.
# For test displays (Xvfb): it replaces the whole property, so it refuses a display whose resources hold anything
# else (a real desktop's: set the scale through the desktop there, e.g. vm/probe/scale-desktop.sh).
# Usage: xft-dpi.sh <dpi | -> ; "-" removes the resources.
set -euo pipefail
current=$(xprop -root RESOURCE_MANAGER 2>/dev/null | sed -n 's/^RESOURCE_MANAGER(STRING) = //p')
others=$(printf '%s' "$current" | sed -e 's/^"//' -e 's/"$//' -e 's/Xft\.dpi:\\t[0-9.]*//' -e 's/\\n//g')
if [ -n "$others" ]; then echo "xft-dpi.sh: the display has other X resources ($current); not touching them" >&2; exit 1; fi
if [ "${1:--}" = - ]; then xprop -root -remove RESOURCE_MANAGER
else xprop -root -f RESOURCE_MANAGER 8s -set RESOURCE_MANAGER "$(printf 'Xft.dpi:\t%s\n' "$1")"; fi
xprop -root RESOURCE_MANAGER
