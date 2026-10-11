#!/usr/bin/env bash
set -e
W=/Volumes/Storage/code/IPlug2/linux-research/wt/hvoya-trial-rangerrick
g++ -std=c++17 -O2 -fsigned-char -DOS_LINUX -I$W/IPlug -I$W/WDL /Volumes/Storage/code/IPlug2/linux-research/build/review-race/timer_race.cpp $W/IPlug/IPlugTimer.cpp -o /Volumes/Storage/code/IPlug2/linux-research/build/review-race/timer_race -lpthread
/Volumes/Storage/code/IPlug2/linux-research/build/review-race/timer_race
/Volumes/Storage/code/IPlug2/linux-research/build/review-race/timer_race
nproc
