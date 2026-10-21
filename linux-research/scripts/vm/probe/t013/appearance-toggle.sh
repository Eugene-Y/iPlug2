#!/usr/bin/env bash
# The real GNOME portal: switch the desktop's color scheme the way Settings > Appearance does (gsettings
# color-scheme), then back, and show the IPLUG_TEST_APPEARANCE lines the open editor printed to <host-stderr>.
# The user's setting is restored on any exit.
# Usage: appearance-toggle.sh <host-stderr>   (run through vm-x.sh, editor open, built with -DIPLUG_TEST_APPEARANCE)
LOG="$1"; KEY="org.gnome.desktop.interface color-scheme"
orig=$(gsettings get $KEY) || exit 1
trap 'gsettings set $KEY "$orig"; echo "restored $(gsettings get $KEY)"' EXIT
echo "original $orig"
other=default; [ "$orig" = "'default'" ] && other=prefer-dark
gsettings set $KEY "$other"; sleep 2; echo "set $other"
gsettings set $KEY "$orig"; sleep 2; echo "set back $orig"
grep -a IPLUG_TEST_APPEARANCE "$LOG"
