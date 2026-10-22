#!/usr/bin/env bash
# T-022 smoke (APP MIDI follows the JACK driver), run from the branch worktree before the merge: APP builds of
# IPlugInstrument, IPlugControls, Gneiss, Sand; the JACK MIDI harness; IPlugControls P3b in REAPER (CLAP + VST3).
L=/Volumes/Storage/code/IPlug2-T-022; cd $L/linux-research
scripts/dock.sh -- scripts/build-cmake.sh .. build/t022s-ex IPlugInstrument-app IPlugControls-app IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E ' error|RESULT|FAILED'
for p in Gneiss Sand; do T=/Volumes/Storage/code/hvoya/$p-linux
  DOCK_MOUNTS="$T /Volumes/Storage/code/hvoya/$p/.git:ro" DOCK_ENV="EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$L" scripts/dock.sh -- scripts/build-cmake.sh $T build/t022s-$p $p-app 2>&1 | grep -E ' error|RESULT|FAILED'
done
scripts/gui-up.sh t022s >/dev/null
scripts/gx.sh -d t022s bash -c 'jackd --no-realtime -d dummy -r 48000 -p 256 > /tmp/jackd.log 2>&1'; sleep 2
scripts/gx.sh t022s scripts/app-jack-midi-test.sh evidence/merge-smoke/t022m-jack-midi build/t022s-ex/out/IPlugInstrument 2>&1 | grep -E "FAIL|RESULT" | tail -4; echo "== jack midi"
docker exec t022s pkill -x jackd
for f in clap vst3; do
  GUI=t022s scripts/run-gui-test.sh build/t022s-ex/out/IPlugControls.$f evidence/merge-smoke/t022m-controls-$f scripts/steps/iplugcontrols-p3b.txt t022s 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -2; echo "== controls p3b $f"; docker exec t022s pkill -x reaper
done
docker rm -f t022s >/dev/null; echo ALL-DONE
