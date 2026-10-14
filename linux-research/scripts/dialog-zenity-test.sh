#!/usr/bin/env bash
# dialog-test.sh with the portal skipped (IPLUG_LINUX_DIALOGS=zenity): the fallback on a desktop that has a portal.
IPLUG_LINUX_DIALOGS=zenity exec "$(dirname "$0")/dialog-test.sh" "$@"
