#!/usr/bin/env bash
# Re-run every REAPER editor test used in notes/03 with the current harness (host side).
# Usage: [GUI=<container>] rerun-reaper-matrix.sh [filter-regex]   e.g. rerun-reaper-matrix.sh 'rangerrick.*vst3'
# The scripts use fixed paths inside the container (/tmp/rhome, /tmp/reaper.pid) and one X display,
# so concurrent users must each use their own container (GUI=gui-<name>).
set -uo pipefail
cd "$(dirname "$0")/.."
GUI="${GUI:-gui}"
docker inspect -f '{{.State.Running}}' "$GUI" 2>/dev/null | grep -q true || scripts/gui-up.sh "$GUI"
S=scripts/steps/iplugcontrols.txt
while read -r cand fmt name plug steps; do
  [[ "$cand $fmt $name $steps" =~ ${1:-.} ]] || continue
  E=evidence/$cand/gui/reaper-$fmt-$name
  [ "$steps" = "-" ] && { steps=""; E=$E-nosteps; }
  rm -rf "$E" "$E.log"
  scripts/gx.sh "$GUI" timeout 900 scripts/reaper-plugin-test.sh "$E" "$(echo $fmt | tr a-z A-Z): $name" "$plug" $steps < /dev/null > "$E.log" 2>&1
  echo "$cand $fmt $name: $(grep '^RESULT' "$E.log" || echo 'RESULT: (none - harness aborted)')"
done <<EOT
rangerrick vst3 IPlugEffect build/rangerrick/out/IPlugEffect.vst3 -
rangerrick clap IPlugEffect build/rangerrick/out/IPlugEffect.clap -
rangerrick vst3 IPlugControls build/rangerrick/out/IPlugControls.vst3 $S
rangerrick clap IPlugControls build/rangerrick/out/IPlugControls.clap $S
rangerrick vst3 IPlugControls build/rangerrick/out/IPlugControls.vst3 -
rangerrick clap IPlugControls build/rangerrick/out/IPlugControls.clap -
stillwell vst3 IPlugEffect build/stillwell/out/IPlugEffect.vst3 -
stillwell clap IPlugEffect build/stillwell/out/IPlugEffect.clap -
stillwell vst3 IPlugControls build/stillwell/out/IPlugControls.vst3 $S
stillwell clap IPlugControls build/stillwell/out/IPlugControls.clap $S
hvoya-trial vst3 IPlugControls build/hvoya-trial/out/IPlugControls.vst3 $S
hvoya-trial clap IPlugControls build/hvoya-trial/out/IPlugControls.clap $S
hvoya-trial vst3 IPlugControls build/hvoya-trial/out/IPlugControls.vst3 -
hvoya-trial clap IPlugControls build/hvoya-trial/out/IPlugControls.clap -
EOT
