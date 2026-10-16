#!/usr/bin/env bash
# Reviewer load generator: REAPER with an IPlugControls editor open (meters animate, llvmpipe draws ~2 cores).
# Runs inside a GUI container, cwd linux-research. Usage: load-reaper.sh <plugin>
set -uo pipefail
scripts/reaper-up.sh "$1"
sleep 4
scripts/rpr.sh 'reaper.InsertTrackAtIndex(0,true); local t=reaper.GetTrack(0,0); reaper.TrackFX_AddByName(t,"CLAP: IPlugControls",false,-1); reaper.TrackFX_Show(t,0,3); out("ok")'
