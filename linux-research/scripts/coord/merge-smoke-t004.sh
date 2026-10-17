#!/usr/bin/env bash
# Coordinator merge smoke for T-004 (run from linux-research on the trial-merged hvoya-linux).
cd /Volumes/Storage/code/IPlug2-linux/linux-research
E=evidence/merge-smoke; B=build/m004
scripts/dock.sh -- scripts/build-cmake.sh .. $B IPlugEffect-app IPlugInstrument-app IPlugControls-app IPlugControls-clap IPlugControls-vst3 2>&1 | grep -E " error|RESULT|FAILED" ; ls $B/out
scripts/gui-up.sh m004 >/dev/null
for a in IPlugEffect IPlugInstrument; do
  scripts/gx.sh m004 timeout 300 scripts/app-audio-test.sh $E/t004-audio-$a $B/out/$a 2>&1 | grep -E "FAIL|RESULT|PASS" | tail -4; echo "== audio $a exit $?"
done
scripts/gx.sh m004 bash -c "(jackd --no-realtime -d dummy -r 48000 -p 256 >/tmp/jackd.log 2>&1 &); sleep 1.5; timeout 300 scripts/app-switch-test.sh $E/t004-switch-IPlugInstrument $B/out/IPlugInstrument PulseAudio; echo exit \$?; pkill -x jackd" 2>&1 | grep -E "FAIL|RESULT|PASS|exit" | tail -4
GUI=m004 scripts/run-gui-test.sh $B/out/IPlugControls $E/t004-app-controls scripts/steps/iplugcontrols-app-p3b.txt m004 2>&1 | grep -E "FAIL|DEAD|RESULT" | tail -5; echo APP-P3b-done
GUI=m004 scripts/run-gui-test.sh $B/out/IPlugControls.clap $E/t004-controls-clap scripts/steps/iplugcontrols-p3b.txt m004 2>&1 | grep -E "xact\] FAIL|RESULT" | tail -3
docker rm -f m004 >/dev/null; echo ALL-DONE
