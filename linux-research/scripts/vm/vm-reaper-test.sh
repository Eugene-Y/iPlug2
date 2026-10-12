#!/usr/bin/env bash
# reaper-plugin-test.sh on the VM's real GNOME desktop: open, steps, reopen x5, 2nd instance, main-thread check.
# Usage: vm-reaper-test.sh <out> "<FMT: Name>" <plugin on the VM> [steps]
#   <out>, [steps]: relative to linux-research/ (e.g. evidence/vm-p3a/reaper-controls-clap, scripts/steps/iplugcontrols-p3a.txt)
#   <plugin>: e.g. ~/.clap/IPlugControls.clap, ~/.vst3/IPlugControls.vst3 (where vm-deploy.sh puts them)
# Env: see vm-run.sh.
exec "$(dirname "$0")/vm-run.sh" reaper-plugin-test.sh "$@"
