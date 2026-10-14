#!/usr/bin/env bash
# Set or remove Xft.dpi in the X resources (RESOURCE_MANAGER on the root window), as GNOME/KDE do, without xrdb.
# Usage: xft-dpi.sh <dpi | -> ; "-" removes the resources. Runs where DISPLAY points (the GUI container, the VM).
set -euo pipefail
if [ "${1:--}" = - ]; then xprop -root -remove RESOURCE_MANAGER
else xprop -root -f RESOURCE_MANAGER 8s -set RESOURCE_MANAGER "$(printf 'Xft.dpi:\t%s\n' "$1")"; fi
xprop -root RESOURCE_MANAGER
