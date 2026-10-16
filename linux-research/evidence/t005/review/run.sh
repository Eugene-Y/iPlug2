#!/usr/bin/env bash
set -u
R=/Volumes/Storage/code/IPlug2-T-005/linux-research/evidence/t005/review
OUT=/Volumes/Storage/code/IPlug2-T-005/Examples/IPlugControls/build/linux-debug/out
mkdir -p ~/.clap ~/.config/REAPER
rm -rf ~/.clap/IPlugControls.clap ~/.clap/IPlugControls.resources
cp -a $OUT/IPlugControls.clap $OUT/IPlugControls.resources ~/.clap/
printf '[REAPER]\nlinux_audio_mode=2\nsplashscreen=0\n' > ~/.config/REAPER/reaper.ini
pgrep -x jackd >/dev/null || { jackd --no-realtime -d dummy -r 48000 -p 256 >/tmp/jackd.log 2>&1 & sleep 1.5; }
export IPLUG2_REAPER_FX="CLAP: IPlugControls"
gdb -batch -x $R/gdb-bp.gdb --args /opt/REAPER/reaper -new /Volumes/Storage/code/IPlug2-T-005/Scripts/linux/reaper-open-fx.lua > $R/gdb-reaper.log 2>&1
