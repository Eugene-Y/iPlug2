# T-006 evidence: clap-validator `state-reproducibility-*`

clap-validator 0.4.1 everywhere (Docker image on Linux; built from the 0.4.1 tag with Homebrew cargo on the Mac).
"Before" = `hvoya-linux` baaecb936, "after" = `linux/T-006` 46d10516c (rescan after `stateLoad`). Builds: Linux examples **Release** (`build/t006-base`; `BUILD_TYPE=Debug` before `dock.sh` does not reach the container), Linux Gneiss **Debug** (`linux-research/build/t006-gneiss`, via `DOCK_ENV`), mac IPlugEffect **Debug**. The reviewer re-ran the examples in both configs with the same results.

| File | Platform | Plug-in | Result |
|---|---|---|---|
| `before-{IPlugEffect,IPlugControls,IPlugInstrument}.txt` | Linux | examples, before | `-t state`: 3 of 5 failed each — "After reloading the state, these parameter values changed without a rescan request" |
| `after-{IPlugEffect,IPlugControls,IPlugInstrument}.full.txt` | Linux | examples, after | full run: 0 failed / 0 failed / 1 failed (`param-fuzz-bounds`, subnormal output, known since notes/03, not state) |
| `after-Gneiss.full.txt` | Linux | Gneiss `linux-experiment` 4219350, after | all state tests pass (binary equality included); 4 other failures, all known (T-003 Log, C3, C5) |
| `mac-before-IPlugEffect.full.txt` | macOS | IPlugEffect, before | 3 failed: the three state tests, same message |
| `mac-after-IPlugEffect.full.txt` | macOS | IPlugEffect, after | 44 run, 0 failed |
| `mac-gneiss-installed.full.txt` | macOS | the user's installed Gneiss CLAP (Oct 16, from `hvoya`), read-only | the three state tests fail (71 params "changed without a rescan request"); also `process-audio-double-*` ×2 (C3), `param-conversions` (C5) |
| `reaper-gneiss-clap/`, `reaper-iplugcontrols-clap/` | Linux, REAPER 7.82 | after | `scripts/steps/state-roundtrip.txt`: set param 0 → save chunk → move it → `SetTrackStateChunk` → param 0 restored, REAPER alive, draws on tid 1. PASS both. IPlugControls `after-load.png` shows Gain at 25 % (restored) rather than 90 % |
