#!/usr/bin/env bash
# dialog-test.sh expecting the xdg-desktop-portal FileChooser (the VM): for vm-run.sh, which passes no environment.
EXPECT_PORTAL=1 exec "$(dirname "$0")/dialog-test.sh" "$@"
