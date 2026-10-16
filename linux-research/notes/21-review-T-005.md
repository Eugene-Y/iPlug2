# 21 — Review of T-005 (round 1, 2026-10-16)

Independent fresh-context review of `hvoya-linux..linux/T-005` (1411474a2, cdb79d56c, 1c5f3f480, c1d8d7ce6).
My own outputs are in `evidence/t005/review/` (PNGs git-ignored; `gdb-bp.gdb`, `gdb-reaper.log`, `run.sh` left
untracked for the worker to keep or drop).

## What I re-ran

| Check | Result |
|---|---|
| mac: `cmake --list-presets` at root | same 10 presets as `hvoya-linux` (identical output, md5), rc 0 |
| mac: `cmake --list-presets` in `Examples/IPlugEffect`, `Examples/IPlugControls` | empty list, rc 0 |
| mac: `cmake --preset macos-ninja -B build/t005r-macos-ninja -DIPLUG_DEPLOY_PLUGINS=OFF` | configures, `NANOVG/METAL` (build dir removed afterwards) |
| JSON validity of the 2 preset files, 2 example preset files, 2 workspaces | all parse |
| Docker: `cmake --preset linux-debug && cmake --build --preset linux-debug` in `Examples/IPlugControls` | PASS, 121 steps, CLAP + VST3 deployed, GCC 13.3 (build dir removed afterwards) |
| Docker GUI (`t005r`): REAPER under `gdb -batch`, `IPLUG2_REAPER_FX="CLAP: IPlugControls"`, `-new Scripts/linux/reaper-open-fx.lua`, pending bp `IPlugControls::ProcessBlock` | **bp hit** (`IPlugControls.cpp:663`, via `IPlugCLAP::process`); full editor open (`review/reaper-iplugcontrols.png`, looked at) |
| Pristine `ubuntu:24.04` container + **only** the docs' apt list, `cmake --preset linux-debug -B /tmp/b` in `Examples/IPlugEffect` | PASS, 154 steps, APP + CLAP + VST3 built. The docs' package list is complete. |
| clang 18.1.3 (Ubuntu) predefines `__FLOAT_WORD_ORDER__`? | no (GCC does). The `.clangd` rationale holds. |
| JetBrains non-commercial licence | v3.0, effective 2025-12-16, §2.3 + §3.2(B)(vi): matches notes/20 |
| `duplicate.py IPlugEffect MyFx Mfr <scratch>` (out of tree) | matches the Log: `build/` copied (172 MB), `../..` left in presets, workspace and CMakeLists |
| `duplicate.py IPlugEffect T005RFx Mfr` (**in tree**), then `cmake --preset linux-debug` | **fails**, see M1 |
| REAPER `linux_audio_mode` 0/1/2/3 in Docker with `jackd -d dummy` running | 0 = JACK, 2 = **Dummy Audio**, see M2 |
| Second `reaper -new` while one runs; `reaper -newinst -new` | first hands off and exits; `-newinst` stays, see m1 |

Screenshots I opened: 04, 07, 10, 11, 12, 20, 22, 23, 25, 26, 27, 30, 31 (vm/) and lua-open-fx.png (docker/).
They show what the Log claims, except 31 (m4). 20/22 (CLAP), 23/25 (APP) and 26 (VST3) each show the paused
frame at the right line, the right thread kind and the plug-in binary from `~/.clap` / `~/.vst3`. 07 shows clangd idle with inlay
hints in `IControls.cpp` and "No problems". 27 shows the full IPlugControls editor.

The mac/win rule holds for builds. No Xcode/VS project, xcconfig, props, `config.h` or C++ source is touched. The
root `CMakePresets.json` change is Linux-conditioned and the mac preset list is byte-identical. The root file was already
schema v6, so `include` raises no CMake minimum. CI (`cmake-ci.yml`) uses only root mac/win presets.
Comments (`.clangd`, `reaper-open-fx.lua`) are WHY-only. `SHOW_FLOATING` is a named constant instead of a comment.

## Findings

### Major

**M1. Duplicating a built template gives a project that cannot configure.**
`Scripts/cmake/Linux/CMakePresets-linux.json:8` puts builds in `${sourceDir}/build/${presetName}`. `Examples/duplicate.py:42`
`DONT_COPY` skips `build-*` but not `build`, and `copytree` runs for in-tree duplication too. So after anyone has
built IPlugEffect with the presets, `duplicate.py IPlugEffect T005RFx Mfr` copies `build/linux-debug/CMakeCache.txt`, and
`cmake --preset linux-debug` in the new project fails:
"The current CMakeCache.txt directory …/T005RFx/build/linux-debug … is different than … IPlugEffect/build/linux-debug"
and "source … does not match the source … used to generate cache". Reproduced in Docker, then deleted. CMake Tools would hit the same
error on first open. This breaks the workflow the user asked for: templates to develop in and duplicate from.
`Documentation/linux-development.md:108-111` and `notes/20-ide-research.md:83-86` say in-tree duplication is fine and only
out-of-tree copies `build/`. That is wrong: in-tree copies it too.
*Fix (one line, no shared-script change):* a binaryDir that `DONT_COPY` and `.gitignore` already cover, e.g.
`${sourceDir}/build-linux/${presetName}` (root `.gitignore:31` `build-*`; examples `Examples/*/build*`). Point
`cmake.copyCompileCommands` and `--compile-commands-dir` at `build-linux/`, and update docs §2/§3 and notes/20.
(Adding `build` to `DONT_COPY` also works, but that changes an upstream script, which is T-011's business.)

**M2. `scripts/vm/README.md:62-65` now states a wrong fact: `linux_audio_mode=2` is Dummy Audio, not JACK.**
In the `iplug2-linux:24.04` container, with `jackd --no-realtime -d dummy -r 48000 -p 256` running and `reaper.ini`
`linux_audio_mode=2`, REAPER's status reads "44.1kHz … 512spls … **Dummy Audio**" (`review/reaper-status.png`).
`jack_lsp` shows no REAPER ports. With `linux_audio_mode=0` it reads "48kHz 24bit WAV : 2/2ch 256spls … **JACK**" and
registers `REAPER:out1`… (`review/mode0.png`). Modes 1 and 3 give "Error opening devices" in the container, which
fits ALSA and PulseAudio. So the order is the Preferences dropdown order: 0 JACK, 1 ALSA, 2 Dummy, 3 PulseAudio. The
worker's own `docker/lua-open-fx.png` also shows "Dummy Audio" with mode 2.
The VM screenshots 10–12 ("Error opening devices", JACK selected) are most likely the root-run REAPER the Log
describes: root's HOME has no seeded ini, so REAPER falls back to its default, JACK. The original README sentence ("mode 2 is
REAPER's dummy audio; the harness's jackd dummy is started but unused") was correct. The parenthesis "without jackd,
mode 2 gives 'Error opening devices'" is false.
*Fix:* restore the original statement, add the mapping (0 JACK, 2 Dummy, 3 PulseAudio), keep the sudo-gdb gotcha.
Correct the Log line `tasks/T-005-ide-research.md:82`.
Two older comments say the same wrong thing. They are not from T-005, but they sit in files next to this one, so fix them
or flag them for the coordinator: `scripts/vm/vm-provision.sh:6` ("JACK audio") and `scripts/reaper-up.sh:22` ("JACK
dummy backend so REAPER … run a real audio thread").

### Minor

**m1. "Debug CLAP/VST3 in REAPER" does nothing useful if REAPER is already running.**
A second `reaper -new …` hands its arguments to the running instance and exits (verified: second process gone, the first
shows "[proj 2/2]"). gdb then reports a normal exit, and the plug-in loads into the undebugged REAPER, so no breakpoint
can hit. Developers often keep REAPER open. `reaper -newinst -new` keeps the second instance (verified).
*Fix:* add `"-newinst"` to `args` at `Examples/*/*-linux.code-workspace:75` and `:92`, and to the Qt Creator arguments in
`Documentation/linux-development.md:99`.

**m2. The examples' `CMakePresets.json` changes CMake Tools behaviour on mac/win.**
CMake Tools' default `cmake.useCMakePresets: auto` switches to presets mode whenever `CMakePresets.json` exists. On mac/win
every preset in the examples is Linux-conditioned, so a mac/win developer who opens `Examples/IPlugEffect` with CMake
Tools now gets an empty preset list instead of the kits workflow. This is tooling only: builds are unaffected, and the
mac `IPlugEffect.code-workspace` does not use CMake Tools. I did not reproduce it in VS Code on mac; it follows from
CMake Tools' documented `auto` rule.
Options for the user: accept and note it. Or have the examples include `../../CMakePresets.json` instead of the Linux
file directly; I checked that the root presets list correctly when included from another directory. That gives mac/win the root's presets
per example, which is additive but a visible change for mac/win. Not a blocker; worth one line in the docs or the coordinator's
decision list.

**m3. The REAPER path can only be overridden by editing a tracked file.**
`iplug2.reaperPath` is set in the workspace (`*-linux.code-workspace:13`). Workspace settings take precedence over user
settings, so a developer whose REAPER is elsewhere (e.g. `~/opt/REAPER` from `install-reaper.sh` without sudo) must edit the
checked-in workspace and carry a permanent diff, as `Documentation/linux-development.md:25` says. Mention the
no-diff alternative in the docs: a `/opt/REAPER` symlink, which the VM harness already uses.

**m4. The evidence for "Qt Creator configures" is weak.**
`vm/31-qtcreator-configured.png` shows the project loaded with the Debug kit, but the Compile Output is empty, the tree is
collapsed and no CMake messages are visible. Nothing in the picture proves the configure succeeded. Low stakes (the
alternative IDE, "not driven further"), but either show General Messages ("Build files have been written…") or
say "loads the presets as kits" instead of "configures" (`notes/20-ide-research.md:68-69`, Log line 76).

### Nits

- `Documentation/linux-development.md:82`, `notes/20-ide-research.md:52`: "`ProcessBlock` stops on REAPER's audio
  thread". In 20 and 26 it stops on `reaper/mediafx` (anticipative-FX worker), not `reaper/audio`. Say "on a REAPER
  audio/FX worker thread".
- The root now has hidden `base-linux` (upstream) and hidden `linux-base` (new): swapped names for different things, and
  GL2 stated in both. Consider a distinct name such as `linux-dev-base`.
- Task labels "Build and deploy CLAP/VST3" (`*-linux.code-workspace:35,44`) only deploy while `IPLUG_DEPLOY_PLUGINS=ON`.
  The docs (§3) tell users how to turn it off. Harmless.
- The per-example `.clangd` copies are redundant in-tree (clangd also applies the root one, so the flag is added twice,
  harmlessly). They are justified for out-of-tree projects (Gneiss, Sand), whose TUs have no iPlug2 ancestor, so keep them.
  Repeating the 2-line WHY in each copy is unavoidable.

## Acceptance list

- notes/20 with table, sources, recommendation: yes. Licence claims for CLion spot-checked and correct.
- Presets and IDE configs checked in, mac/win presets unaffected, `cmake --list-presets` on mac: yes. m2 is a CMake-Tools-only
  side effect.
- VM build plus both breakpoints hit in the recommended IDE, with screenshots: yes (20, 22, 23, 25, 26).
- Docs written: yes, and the install list is verified complete on a pristine image. §6 is wrong (M1).

Verdict: changes requested

## Round 2 (2026-10-16, re-check of d3d1ac762)

| Round-1 item | Re-check | Status |
|---|---|---|
| M1 stale CMakeCache in duplicates | Docker: built `Examples/IPlugEffect` with `linux-debug` (now `build-linux/linux-debug`), `duplicate.py IPlugEffect T005R2Fx Mfr` in tree. The duplicate has no build folder, `cmake --preset linux-debug` gives rc 0, and the build produces APP/CLAP/VST3 (154 steps). `git status` shows the duplicate's `build-linux/` as ignored. Duplicate deleted afterwards. | fixed |
| M2 `linux_audio_mode` | `scripts/vm/README.md` gives 0 JACK / 2 Dummy / 3 PulseAudio, which matches my round-1 measurements. The `reaper-up.sh:22` comment now gives the WHY (jackd unused under mode 2, needed for mode 0), `vm-provision.sh:6` says Dummy Audio, and the Log line is struck through and corrected. | fixed |
| m1 `-newinst` | Both workspaces (`:75`, `:92`) and the Qt Creator args in the docs. Docker: a REAPER already running, then `gdb … reaper -newinst -new reaper-open-fx.lua` exactly as in the workspace. 2 instances; `IPlugEffect::ProcessBlock` hit in the debugged one; editor open, built from d3d1ac7 (`evidence/t005/review2/`, screenshot looked at). | fixed |
| m2 CMake Tools presets mode on mac/win | unchanged, recorded as the user's call in the Log | open (user decision) |
| m3 REAPER path | docs: `/opt/REAPER` symlink, and that workspace settings beat user settings | fixed |
| m4 Qt Creator evidence | notes/20 now says the CMake output was not captured | fixed |
| nits | docs say `reaper/mediafx`; hidden preset renamed `linux-common` | fixed |

Also checked:
- **mac presets:** `cmake --list-presets` at root is still identical to `hvoya-linux`.
- **JSON:** the changed preset and workspace files parse.
- **`.gitignore` coverage of `build-linux`:** covered by root `build-*`, `Examples/*/build*`, and `gitignore_template`'s `build-*`.
- **T-011 task file:** it is self-contained. It correctly drops the `build/` item and keeps the out-of-tree `../..` rewrite, Gneiss and the mac before/after diff as acceptance.

New nits, no action needed: `Documentation/linux-development.md:84` is now one long line (the `reaper/mediafx` edit).
The committed `evidence/t005/review/run.sh` still names the old `build/linux-debug` path. It is a record of round 1, so leave it.

Verdict: accept
