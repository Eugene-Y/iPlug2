# T-026 — Sand: follow-ups after T-024 (N1–N4, F2 host checks, docs)

- **Status:** review-done (independent review: round 1 accept with minor fixes, round 2 pass, 2026-10-18); hand-over at the end of the Log
- **Owner:** T-026 chat (2026-10-17). Task file, Log and evidence: iPlug2 branch `linux/T-026` (from `hvoya-linux` ab632ec4e) in `/Volumes/Storage/code/IPlug2-T-026-log`. Lock `.linux-port-locks/T-026`.
- **Where the work is:** Sand `fix/t026-followups` in `hvoya/Sand-t026` (N1 `9c2dad2`, N3 `cc81d9d`); iPlug2 `fix/host-info-beat` in `IPlug2-T-026` (T21-5 proposal `b36d0d545`). Details: Log, "Where everything is".
- **Branches/worktrees:**
  - Sand repo: branch `fix/t026-followups` from `master` (54be993 or newer), in a new worktree `/Volumes/Storage/code/hvoya/Sand-t026` (`git -C /Volumes/Storage/code/hvoya/Sand worktree add ../Sand-t026 -b fix/t026-followups master`). Never the main checkout `/Volumes/Storage/code/hvoya/Sand` (the user's working tree; its untracked `Media/` stays). Never push; the coordinator merges at the user's command.
  - iPlug2/hvoya only if an item needs shared code (item 6): branch from `hvoya` in `/Volumes/Storage/code/IPlug2-T-026`. Never `/Volumes/Storage/code/IPlug2` itself (Gneiss builds against it).
- **Depends on:** T-024 (merged 2026-10-17: Sand master 54be993, hvoya 083bc87c5/228f2efc2/a865ee9f7), T-021 (merged: `Scripts/linux/package.sh`, runtime smoke images).
- **Conflicts:** Sand sources/projects; nothing in the Linux port files.
- **Not Linux work:** these are product fixes for every platform. AGENTS §4 (independent reviewer) and §5 (locks) apply; AGENTS §2's "don't fix cross-platform bugs in port branches" does not, but its spirit does: smallest correct change, no unrelated refactors, each behaviour change explained in its commit message.

## Read first
- `AGENTS.md`, Sand `CLAUDE.md` (its "do not build" line predates 2026-10-17: **building is allowed**, user), `README.md`, `BACKLOG.md`.
- `tasks/T-024-sand-cross-platform-fixes.md` Log: N1–N4 (~line 177), F2 residual risk (~line 222), reviews R6/R7; `tasks/T-021-release-packaging.md` Log: T21-1, T21-5; `CROSS_PLATFORM_FINDINGS.md` sections "Из T-012" and the T-024 summary.
- T-024's helpers in `linux-research/evidence/t024/`: `run-checks.sh` (clap-validator, VST3 `-e` ×20, ASan VST3 ×3 + CLAP, renders), `mac-checks.sh`, `render-clap.cpp` (offline CLAP host: output, non-finite samples, input-buffer change, process() time), `cpu-bench.sh`, `xcb-sand.sh` (command-line xcodebuild with every install path in a scratch dir). Its `build-sand.sh` was git-ignored (`build-*`) and lost; the coordinator recreated it as `scripts/sand-build.sh <tree> <builddir> [release|asan] [targets]` (2026-10-17: release and ASan builds of Sand-linux PASS; ASan VST3 validator 537/537, no ASan report).

## Items

### 1. N1 — allocation on the audio thread (fix; highest value)
`SandModule::process` copy-constructs `hvoya::AudioBuffer dryCopy (processed)` on every block (`sand_module.hpp:304`), which allocates in `resize()`. Sand's CLAUDE.md forbids heap allocation in the audio path; at small buffers this risks dropouts on every platform.
- Make `dryCopy` a member (or a pre-sized scratch buffer) sized for the maximum block × channels in the reset path (`OnReset`/`prepare`-like code that already sizes Sand's other buffers), and fill it per block without allocation (`fillFrom`/copy into the existing storage; keep `assert(!isWrapper())` semantics where they matter).
- **Audit the whole audio path** (`Sand::ProcessBlock` → `SandModule::process` → grind/diffusor/filters/oversampler/envelope paths and hvoya helpers they call) for any other allocation: copies of `AudioBuffer`, `std::vector` growth, `std::function` captures, string formatting, logging. List each in the Log with file:line and fix the Sand-side ones; shared hvoya ones → item 6.
- **Proof:** an allocation counter in a Linux test build (e.g. `LD_PRELOAD` malloc/realloc/calloc counter or a `malloc` hook armed only inside `ProcessBlock`) over the offline renderer with several block sizes, rates and OS factors, including a block-size change: 0 allocations after warm-up, before vs after. Audio identical before/after (`render-clap.cpp`, bit-exact or explained). CPU before/after (`cpu-bench.sh`).

### 2. N2 — fresh clone builds on mac (verify, likely closed)
The macOS `resources/Sand-*-Info.plist` were untracked (9d0c8ea / 05c854b) and `prepare_resources-mac.py` only updates existing plists. The user's 961be6c "puts back plists" tracks them again and drops the `.gitignore` line.
- Verify: `git clone` Sand master into the scratchpad (sibling `IPlug2` must resolve: clone into a scratch folder that also holds a symlink or worktree of iPlug2 `hvoya` at `../../IPlug2` relative to `projects/`, or pass `IPLUG2_ROOT`), then `xcb-sand.sh` CLAP, VST3, AU, APP. Record the result; if anything is still missing, fix it the way the user's commit chose (tracked plists), nothing else.

### 3. N3 — `Sand-aax.vcxproj` is stale (fix the file list)
It lacks `SandUI.cpp`, every hvoya source and (after F7) `PolyphaseIIR2Designer.cpp`. Bring its source list (and `.filters`) in line with `Sand-vst3.vcxproj`, plus whatever AAX-specific sources iPlug2's AAX template lists (compare with `IPlug2/Examples/IPlugEffect/projects/IPlugEffect-aax.vcxproj`). You cannot build on Windows/AAX here: say so in the commit; the user verifies. Don't touch other project settings.

### 4. N4 — FL Studio crash (prepare the user's check)
BACKLOG: "Sand crashes 100% in FL Studio when adjusting the filter settings, particularly the mix knob" (Win 10). Candidates fixed in T-024: F1 (FL sends 0-frame VST3 flushes), F3 (≥ 88.2 kHz with 8x). Without FL here:
- try to reproduce the class of the crash on Linux/mac: VST3 validator ×20 and the offline renderer with automation sweeps of the filter params and mix at 44.1/48/88.2/96/192 kHz × OS 1/2/4/8, 0-frame blocks interleaved, ASan build on Linux;
- write a short checklist for the user's FL test (version, rate, OS factor, which knob, what to send back) into the Log and BACKLOG wording proposal (BACKLOG is the user's living file: propose the edit in the Log, don't edit it).

### 5. F2 follow-up — CC-driven values in hosts other than REAPER (verify)
After T-024 the mediator reports CC-driven values from `OnIdle`. Verify on mac: REAPER (CLAP, VST3, AU; scriptable with ReaScript like the Linux harness), `auval -v` for the AU, and — if you can drive them — Bitwig (CLAP, VST3) and Ableton Live Lite (AU, VST3); otherwise write a 5-minute manual checklist for the user. Check: the host's parameter display and automation recording follow CC; Sand's knob follows within one idle tick; no echo step-back at the end of a fast CC sweep in VST3 (residual risk: a value stepped back for ≤ 20 ms). If the step-back reproduces, propose a fix (e.g. ignore the host's echo of a value Sand itself reported in the same tick) — shared code → item 6.

### 6. Shared hvoya/iPlug items → proposal first
Anything in the above that lives in shared code (hvoya utils, iPlug), plus, if you get to it:
- **T21-5** — `hvoya/hvoya/utils/host_info_model.hpp:187`: the displayed beat is right only at 120 BPM (4/4, one quarter in: beat 4 at 40 BPM, 3 at 60, 2 at 120, 1 from ~134). Display only, every hvoya plug-in. Root cause + fix proposal with a small table test (Apple clang and GCC).
Write each as a proposal (commit on the iPlug2 branch, behaviour change per plug-in and format, a row in `CROSS_PLATFORM_FINDINGS.md`) and wait for the user's decision before the coordinator merges.

### 7. Docs: Sand `CLAUDE.md` Grind section (propose)
R6 from the T-024 review: the Grind section says "Two parallel bands of 4 delay lines" and "SR scaling `sr * 0.6 / 1000`"; `dmt_grind.hpp` has 5 bands (`lfoFreq*` size 5), `srScale = sr * 0.6 / 10000`, and `dmt_grind_cfg::maxSR` 3.07 MHz (with T-024's guard) is not mentioned. Also the "do not build" line (item "Read first"). `CLAUDE.md` is the user's instruction file: put the exact proposed text in the Log; apply it on your branch only if the user says so.

## Testing (every code item)
- Linux (Docker): build from a throwaway local branch that merges Sand `linux-experiment` into your branch (never commit that merge to either), like T-024; or package with `Scripts/linux/package.sh <that tree>` and smoke with `scripts/package-smoke.sh` (images `iplug2-rt:*`). clap-validator (known Sand list: `scripts/known-clap-failures/Sand.txt`), VST3 validator ×20, ASan build, REAPER `scripts/steps/sand.txt` CLAP + VST3.
- mac: `xcb-sand.sh` CLAP/VST3/AU/APP into the scratchpad (nothing in `~/Library`), clap-validator 0.4.1 (build it in your scratchpad with Homebrew `rust`, T-006 Log), `auval`.
- Restore anything a build regenerates in a tracked file (e.g. `main.rc_mac_dlg`) after a snapshot (`git add -A && git stash store -m … "$(git stash create)"`), never commit it by accident.

## Acceptance
- [ ] N1 fixed with the allocation proof (0 after warm-up), audio identical, CPU recorded; audit table of the audio path in the Log.
- [ ] N2 verified (fresh clone builds) or fixed minimally.
- [ ] N3 file list fixed; Windows build left to the user, stated.
- [ ] N4: Linux/mac stress results + the user's FL checklist.
- [ ] F2 host checks done (or the manual checklist), step-back answered.
- [ ] Shared items written as proposals; CLAUDE.md text proposed in the Log.
- [ ] One commit per item, messages say what changes on mac/win; main checkouts untouched; nothing pushed.
- [ ] Independent review (AGENTS §4); evidence in `evidence/t026/`; Log complete.

## Log
(append: date — what was done — commits — next)

### 2026-10-17 — all seven items worked; N1 and N3 committed, T21-5 proposed; review pending

Worktrees (all new, nothing in the main checkouts):
- `hvoya/Sand-t026` — `fix/t026-followups` from master 54be993: the product commits.
- `IPlug2-T-026` — `fix/host-info-beat` from hvoya 228f1ea8e: the T21-5 proposal; also the iPlug2 used by every mac build here (SDKs installed with `install-sdks.sh`).
- `IPlug2-T-026-log` — `linux/T-026` from hvoya-linux ab632ec4e: this file, `evidence/t026/`. Linux builds use this tree as `IPLUG2_DIR`.
- Throwaway, never to merge: `hvoya/Sand-t026-base` (detached linux-experiment a027e6d = master + Linux CMake, the Linux "before"), `hvoya/Sand-t026-linux` (`tmp/t026-linux` = fix branch + linux-experiment), `hvoya/Sand-t026-pre` (detached cec9c1d, linux-experiment before T-024: the FL-crash class), `hvoya/Sand-t026-diag` (`tmp/t026-diag`: F2 diagnostic logging).
- Tools in the scratchpad: clap-validator 0.4.1 and the VST3 3.8.1 validator copied from T-024's scratchpad; the mac layout `scratchpad/mac/root/{hvoya/Sand (rsync copy of Sand-t026), IPlug2 → IPlug2-T-026}` with every xcodebuild path there (`evidence/t024/xcb-sand.sh`). Nothing in `~/Library`.
- `Sand-t026/resources/main.rc_mac_dlg`, regenerated by the first mac build (made from a symlink, which Xcode resolved to the main `IPlug2` checkout: read only, same commit 228f1ea8e, `git status` clean before and after), was restored after the snapshot "T-026: before restoring regenerated main.rc_mac_dlg (mac branch build)". All later mac builds are from the rsync copy.

New tools in `evidence/t026/`:
- `render-t026.cpp` — T-024's offline CLAP host plus: block-size lists (`blocks=1024,64,333`), 0-frame blocks carrying events (`zeroevery=N`) and null channel pointers (`zeronull=1`, what VST3 hosts such as FL Studio send for parameter flushes), per-block parameter sweeps (`sweep=id:lo:hi[:i]/…`), a deactivate/activate at another rate and max block (`react=`), and on Linux a malloc/calloc/realloc/memalign counter armed only around `plugin->process()` on the calling thread (`bt=N` prints backtraces; `-DT026_NO_ALLOC_HOOK` for ASan runs).
- `run-allocs.sh` (N1 matrix, Linux), `run-allocs-debug.sh`, `symbolize.sh`, `run-stress.sh` (N4, Linux), `mac-checks.sh` (renders before/after, VST3 ×20, clap-validator), `n3-sync-aax-sources.py`, `f2-sweep-steps.txt` + `f2-stepback.py` (F2), `t21-5-beat-table.cpp`.
- Large outputs (raw renders, full validator logs, full REAPER stderr) stay in the session scratchpad (`evidence-big/`); the repo keeps hashes, summaries and excerpts.

#### 1. N1 — allocation on the audio thread: fixed (Sand `9c2dad2`)

Audit of the audio path, `Sand::ProcessBlock` → `SandModule::process` and everything they call (file:line on `fix/t026-followups` / hvoya 228f1ea8e). "Measured" = seen by the counter (Release unless noted):

| Site | Allocates when | Before | After |
|---|---|---|---|
| `sand_module.hpp:304` (master) `AudioBuffer dryCopy (processed)` | every block, every format | measured: 1 per block | `:317` view into the member `_dryBuf`, reserved in `prepare` — 0 |
| `Sand.cpp:108-109` `_osBuffer.setNumChannels/setNumFrames` | first block after reset and every larger block × os | measured (first block, os change) | reserved in `Sand::prepareProcessBuffers` (`Sand.hpp:180`) — 0 |
| hvoya `oversampler.hpp:163/202` `ensureCapacity` (filters + scratches) | first upsample/downsample, larger block, more channels | measured: 10 allocations on the first os block | `Oversampler::prepare` from `prepareProcessBuffers` — 0 |
| `sand_module.hpp:187-188` `_tempBuffers` (7), `:194` `_cosRampBuf`, `:914` `_grindBuf`, `:1083` `_dryHpBufs` | block longer than 1024 frames (os ≥ 2x at 512) | measured | `SandModule::prepare` (`:170`) reserves them — 0 |
| `Sand.cpp:105` `applyOversampling` on the audio thread (os param change) | `setSampleRate`/`reset` of the module, grind storage reassignment | measured: 13 per change, all from the buffer growth above, none from setSampleRate/reset themselves | 0 (buffers reserved for 8x) |
| `Sand.cpp:95` `HostInfoModel::updateAll`; `Sand.cpp:117` `SoftLimiter::processBuffer`; `SandModule` filters, envelope/Hilbert detectors, `DmtGrind` (inline `std::array` storage), mid/side, companders, `derivativeNthInplace` | never | 0 | 0 |
| MIDI CC: `Sand::ProcessMidiMsg` → `CCMediator::ProcessMidiCC` (`mediator.hpp:127`) → `Mapper::processMidiCC` (`mapper.hpp:97`, lock-free read of the published table) → `OnParamChange` → atomics | never (by reading; not measured: the renderer sends no MIDI) | — | — |
| `Sand::OnParamChange` on the audio thread (VST3/CLAP param events): `isSettingNewVal`, module setters | never in Release | 0 (sweeps of every filter param, mix, os) | 0 |
| plog `LOGD`/`LOGE`: `sand_module.hpp:206` (declickFront), `:339`, `:760` (`setJointMix`), `Sand.cpp:333`, hvoya `host_info_model.hpp:58/71/81` (chans/sr/buf size change), `audio_buffer.cpp:19` (size mismatch), `dmt_grind.hpp:148-153` (`DmtGrind::reset`, runs on every os change on the audio thread), `svf_zdf.hpp:263` (`checkSubnormal`, assert path) | only in logging builds: Release defines `PLOG_DISABLE_LOGGING` on mac (xcconfig), win (props) and Linux (`hvoya.cmake`) | Debug/RelWithDebInfo: measured (`allocs-after/debug-logging-allocs.txt`: first block 10–12; a block-size change 4, from the HostInfoModel "buf size" log; ≈ 3 per joint-mix change) | unchanged — Debug only, the user's own logging; not changed |
| hvoya `midi_cc/mapper.cpp:128` `advanceLearning` → `publishLocked` (`:230`) | the CC that completes a MIDI learn (once per learn, under `try_lock`) | by design, documented in the code | unchanged; shared code → item 6 list, no proposal (one allocation per user learn gesture) |
| iPlug `IPlugAAX.cpp:291` `SetBlockSize` + `OnReset` from the render callback when a block grows | AAX only, iPlug core | pre-existing (iPlug's own scratch buffers) | `OnReset` now also reserves Sand's buffers there; same class, not new. Not measured (no AAX here) |

Fix: the dry copy is a `subBuffer` view of `_dryBuf` (like the other scratch buffers); `Sand::OnReset` → `prepareProcessBuffers()` reserves `_osBuffer` and the module's buffers for `GetBlockSize() × 8` (the factor changes on the audio thread without an OnReset) and calls `Oversampler::prepare`. Every format sets the block size before `OnReset` (VST3 `setupProcessing`, CLAP `activate`, AU `MaximumFramesPerSlice`, VST2 `effSetBlockSize`, AAX, APP). Sand's own buffers still grow on demand for a block longer than the host announced (iPlug's 32-bit scratch does not: review R1, `CROSS_PLATFORM_FINDINGS` T26-b). After review round 1, `prepareProcessBuffers` returns early unless the max block grew (`_preparedMaxFrames`), see the thread-safety note below. Memory held per instance: ≈ 15 buffers × (8 × max block) doubles, ≈ 1 MB at a 1024 max block.

Thread safety (review R2): not every `OnReset` runs while the plug-in is idle — CLAP `reset()` runs on the audio thread, AAX calls `SetBlockSize` + `OnReset` from the render call, and AU calls it from `DoReset` and `kAudioUnitProperty_BypassEffect`, possibly while another thread renders. `reserve` reallocates only when the block grew, and the block grows only in calls made while not processing (VST3 `setupProcessing`, CLAP `activate`, AU `MaximumFramesPerSlice`) or, in AAX, on the audio thread itself. The `_preparedMaxFrames` guard makes the other `OnReset` calls leave the buffers alone entirely. The `updateOversampling` in the same `OnReset` (module reset + rate change while `ProcessBlock` may run) is an older race of that AU path, not touched here: `CROSS_PLATFORM_FINDINGS` T26-c.

Coverage: the counter measures the CLAP wrapper only; the VST3/AU/AAX wrappers' own per-block code is not measured (Sand's code is the same in every format).

Proof (Linux Docker, Release CLAP, `run-allocs.sh`, 47 configurations: 44.1/48/96/192 kHz × os 1/2/4/8, default and every-stage-on, block lists `1024,64,333,512,1`, random blocks with 0-frame blocks, f64 in-place, filter/mix/os sweeps every block, re-activation 48→96 kHz with blocks 4096/128 and 44.1 kHz os 8x with 2048/7):
- before (`allocs-before/allocs.txt`): 1 allocation in every block (316 958 blocks), plus 12–24 in the first block and 13 at each os change;
- after (`allocs-after/allocs.txt`): **0 in every configuration, first block included**;
- audio: **47/47 renders bit-identical** (`render.sha256` before = after);
- non-finite samples: 0.

CPU (`cpu/linux-cpu-n1.txt`, best of 7, builds alternating): 0.1688 → 0.1687 s (100 s f64), 0.1856 → 0.1857 s (block 64), 1.394 → 1.400 s (all stages, os 4x, block 128; the spread between runs is ±5%). No measurable change: glibc's small allocation was cheap on average; the fix removes the worst case (allocator locks, page faults), which a mean does not show.

mac (`mac-branch/`, Apple clang, CLAP Release, branch vs the N2 fresh-clone build of master): 46 render pairs (rates 44.1–192 kHz × os 1–8x, sweeps with null-pointer 0-frame blocks, f64 in-place, os sweep, re-activation) **bit-identical**. The malloc counter is Linux-only: the `allocs=0` in the mac render lines means nothing (only the hashes count).

#### 2. N2 — fresh clone on mac: verified, closed by 961be6c (no change)

`git clone` of master 54be993 into `scratchpad/n2/root/hvoya/Sand`, `root/IPlug2` → `IPlug2-T-026` (hvoya 228f1ea8e): xcodebuild Release **CLAP, VST3, AUv2, APP all exit 0**, every bundle holds a universal (x86_64 + arm64) binary. The tracked `resources/*-Info.plist` (11) are enough; `plug_build_info.hpp` (git-ignored) is generated by the build phase. The build regenerates the tracked `resources/main.rc_mac_dlg` (known; in the throwaway clone).

#### 3. N3 — `Sand-aax.vcxproj`: file list fixed (Sand `cc81d9d`), not built

ClCompile/ClInclude and `.filters` now carry exactly Sand-vst3's non-format sources: + 8 hvoya .cpp, `PolyphaseIIR2Designer.cpp`, `SandUI.cpp`; Sand/hvoya/plog headers; the stale `..\Sand.h` entry goes; filter "hvoya" added. AAX-specific sources (`AAX_Exports.cpp`, `IPlugAAX*.cpp`) were already as in `IPlugEffect-aax.vcxproj`. BOM/LF and all settings byte-identical; both files parse as XML; project and filters list the same items. **Not built (no Windows/AAX here): the user verifies.**

Still missing in that project — settings, so left alone per the task: `LanguageStandard stdcpp23` (+ `stdc17`), which every other Sand vcxproj sets and Sand needs (C++20 `using enum`, `std::span`), and the `PreBuildEvent` that writes `plug_build_info.hpp`. Without them the AAX build will most likely still fail. Ready-made patch (proposal, not applied): `evidence/t026/proposals/n3-aax-settings.diff` (`git apply` in the Sand repo).

#### 4. N4 — FL Studio crash: class reproduced before T-024, nothing after; checklist for the user

- **Before T-024** (Sand cec9c1d, `n4-pre-t024/`): with 0-frame blocks the Release renders survive (iPlug substitutes its scratch buffers for null pointers, so the reads land in the heap), but **the ASan build reports a heap-buffer-overflow in 160/160 runs** on the first 0-frame block: 80 in `SandModule::process` (`sand_module.hpp:376` of that tree, `chan.front()`/`back()`), 80 in hvoya `utils.hpp:132` (`derivativeInplace` on the empty span) (`n4-pre-t024/asan-kinds.txt`, `n4-pre-asan-kinds.sh`). On Windows' allocator that out-of-bounds access can crash. VST3 hosts send such blocks to flush parameters, i.e. exactly when a knob moves with the transport stopped — consistent with "crashes when I adjust the filter settings". This is F1 (fixed in master 54be993).
- **Branch** (`n4-branch-linux/`), filter cutoff/res/type/position/mix + joint/dry/wet/mix mode swept every block, 44.1/48/88.2/96/192 kHz × os 1/2/4/8 × blocks `512` and `64,1,1024,333` × 0-frame blocks before every block or every 3rd × valid or null pointers: **160 Release + 160 ASan renders, 0 crashes, 0 ASan reports, 0 non-finite**. VST3 validator `-e` ×20: 0 failures (537/537 each); ASan VST3 ×3: 0; clap-validator 38/0 (6 skipped), ASan clap-validator 0.
- **mac** (`mac-branch/`): VST3 **Debug** (asserts live) validator ×20: 0 failures; clap-validator 38 pass / 0 fail.
- Not reproduced: the FL-specific threading (FL may call `setParamNormalized` from its UI thread while processing). Sand's setter-thread-only fields (`_jointMix`, `_dryMix`, `_mixMode`) and `_prevParamVals` can then be written from two threads; that is a race on plain doubles (wrong value for a moment at worst), not a crash path I could find.

**Checklist for the user's FL test (Windows 10, FL Studio as in the report):**
1. Build Sand VST3 **Release** from this branch (and, if possible, keep the old v1.1.x installer's VST3 for comparison). Note the FL version (Help > About) and the audio device's sample rate and buffer size.
2. New FL project at 44.1 kHz, Sand on a mixer insert with any audio (a playing sample). Transport **stopped** first: open Sand, move each filter control — cutoff, res, LP/BP/HP, position, then the filter **mix** knob from 1 to 0 and back, slowly and fast — then the main mix knob. Repeat with the transport playing.
3. Repeat step 2 with Sand's oversampling at 2x, 4x and 8x, and once at 96 kHz (FL's audio settings) with os 8x.
4. Repeat steps 2–3 with the old build: if the old one crashes and this one does not, the report is F1 (plus F3 at 96 kHz/8x).
5. If it still crashes, send: FL version, rate/buffer, os factor, which control and whether the transport ran, FL's crash dialog text, and the Windows crash dump or Event Viewer "Application Error" entry (module name + offset).

**Proposed BACKLOG wording** (BACKLOG is the user's file — not edited): replace the FL paragraph's status by a line under it: "2026-10-17: most likely F1 (0-frame parameter-flush blocks read out of bounds; fixed in 54be993) — not confirmed in FL yet; checklist in linux-research tasks/T-026 Log, N4."

#### 5. F2 in other hosts

- **REAPER (Linux 7.82, Docker; diagnostic build, `f2-sweep-{vst3,clap}/`):** CC learn on the comp knob (T-024's menu path), then a MIDI item with CC1 ramps 127→0 in 200 ms, 0→127 in 100 ms, 127→0 in 50 ms, 0→127 in 20 ms (a CC every 2 ms, faster than the 20 ms idle tick), played in automation mode read and then write. The diagnostic build (`tmp/t026-diag`) logs every comp-amount change with its source.
  - VST3: REAPER sends each value Sand reported from `OnIdle` back as a host parameter change in the next block (35 echoes). **All 35 echoes carried the latest CC value** (`f2-stepback.py`: 0 stale), so no step-back was possible; the ordering within a block (params before MIDI) was never needed. Note: iPlug's VST3 wrapper takes only the last point of each parameter queue per block (`IPlugVST3_ProcessorBase.cpp:297`) and MIDI CC reaches VST3 plug-ins through IMidiMapping parameters, so VST3 applies at most one CC per block (71 CC applications against 381 in CLAP for the same item) — this limits how fine a VST3 sweep can be. **No step-back at any of the 8 sweep ends.** Host value and recorded envelope follow (17 points in write mode); Sand's knob matches the host (screenshot `f01-end.png`: comp 0.000 — REAPER itself sends CC1 = 0 at the end of the item).
  - CLAP: no echoes at all, so no step-back is possible.
  - Remaining window, not observed: an idle tick that reads the pending value while the audio thread is already applying the final CC of a sweep; the host's echo of the older value would then be undone by the next tick (≤ 20 ms + one block). No fix proposed: it needs both a fast sweep ending in that sub-block window and a host that echoes; the T-024 fix idea (ignore an echo of a value Sand reported earlier) would add per-param bookkeeping to shared code for an unobserved case.
- **mac — not automated.** REAPER 7.76 with an alternate resource dir (`-cfgfile` in the scratchpad) rescans every system plug-in and shows the evaluation dialog (the license file lives in the user's resource dir), which I cannot dismiss: no GUI control in this session. My two REAPER processes were closed; the user's REAPER config was not touched. The AU of this branch is invisible to macOS unless installed into `~/Library/Audio/Plug-Ins/Components` (forbidden here), and the installed `Sand.component` is the user's 2026-09-13 dev build (before F2), so `auval` of it proves nothing about this branch. Bitwig and Live: no way to drive them here.
- **Manual checklist for the user (≈ 5 min per host)** — install this branch's AU/VST3/CLAP (Release); in each host map MIDI CC 1 to the comp knob with Sand's own MIDI learn (right-click the knob > MIDI CC > Learn, move the controller):
  1. `auval -v aumf Sand HvyA` → "AU VALIDATION SUCCEEDED".
  2. REAPER (AU, VST3, CLAP), Bitwig (CLAP, VST3), Live Lite (AU, VST3): move the controller slowly → Sand's knob follows within a frame or two; the host's own parameter display (REAPER: FX "Param" / envelope lane; Bitwig: device remote/inspector; Live: the device's "Configure" parameter or automation lane) shows the same value.
  3. Arm automation write (REAPER: track mode Write; Bitwig: automation record; Live: Arrangement Record), play, sweep the controller → an automation curve is recorded; play it back in Read → the knob follows the recording.
  4. Fast sweep to an end (slam the controller to 0 or 127) ten times → the knob always ends exactly at the end value, never a step back (watch the knob for 1 s after each slam).
  5. Report host + format for any step that fails.

#### 6. Shared code → proposals (waiting for the user)

- **T21-5 — proposal committed** on iPlug2 `fix/host-info-beat` (from hvoya 228f1ea8e) in `IPlug2-T-026`: `b36d0d545` "hvoya HostInfoModel: the beat number is right at every tempo and time signature".
  - Root cause (`host_info_model.hpp:187`): `GetPPQPos() * 60 / tempo` — PPQ already counts quarter notes, the factor converts it to seconds, so it is right only at 120 BPM; also the beat wrapped at `den` instead of `num` (3/4 showed beat 4, 6/8 beats 7–8) and the fraction was always 0.
  - Fix: `beats = PPQ * den / 4`, `beat = floor(beats mod num) + 1`, fraction = its hundredths.
  - Table test `t21-5-beat-table.cpp` runs the header itself against hand-written expectations (after review R4: 36 cases incl. count-in/negative PPQ, 6/8, 7/8, 2/2 and the fraction, × 6 tempos): **beat or fraction wrong in 168/216 before, 0 after, on Apple clang (`t21-5-apple-clang.txt`) and GCC 13 (`t21-5-gcc.txt`)**; a PPQ a hair below 0 showed beat num+1 (review R5), now the bar start; 4/4 one quarter in: before 4/3/2/2/1/1 at 40/60/90/120/134/180 BPM, after 2 everywhere. Sand CLAP Release builds on mac against the branch.
  - Behaviour change: display only (the host-time line), Sand, Gneiss, Scape, DMT, Melter, every format. Bars still count from PPQ 0 (an in-project time-signature change can offset the beat); iPlug's `ITimeInfo::mLastBar` has no getter, so using it would touch iPlug core — not proposed.
- Smaller shared-code findings, no proposal unless wanted: `host_time_info.hpp` does not include `<cstdint>` (compiles only after other headers; GCC fails on it alone); `HostInfoModel` skips the update at sample position 0 because `_lastSamplePos` starts at 0, and its constructor logs "UI update trigger not set!" when the trigger **is** set (`host_info_model.hpp:35`, Debug log only); the mapper's learn allocation (table above).

#### 7. Sand `CLAUDE.md` — proposed text (not applied; the user's file)

Line 5 (`**Do not build or compile** — the user will build and test locally via Xcode.`) →
> Building is allowed (2026-10-17). Keep command-line build products and plug-in installs out of `~/Library` and the user's checkout (xcodebuild `SYMROOT`/`OBJROOT`/`*_PATH` in a scratch dir), and restore regenerated tracked files such as `resources/main.rc_mac_dlg`. The user still tests in hosts.

Table row `dmt_grind.hpp` →
> | `dmt_grind.hpp` | **Grind diffusor** — allpass network derived from DMT shimmer's grind stages. Five parallel bands of 4 delay lines each (`dmt_grind_cfg::bandLengths`, base lengths 53–887), with cross-band injection between neighbouring bands and LFO-modulated first and last lines. Band outputs are summed, divided by `numBands` and scaled by a grind-dependent gain. Config in `dmt_grind_cfg` namespace. |

Grind section, the "Configurability" bullet's last two sentences (`SR scaling: \`sr * 0.6 / 1000\`. Storage is tightly packed and reassigned on SR change.`) →
> SR scaling: `srScale (sr) = sr * 0.6 / 10000` (as in DMT). Storage is sized for `dmt_grind_cfg::maxSR` = 3.072 MHz (192 kHz × 16), tightly packed and reassigned on SR change; above `maxSR` (host rate × oversampling) line lengths and LFO depth stop growing (`designRate()`).

Also stale since N1 (Oversampling section):
- `user only controls oversampling factor (1x/2x/4x/8x/16x)` → `(1x/2x/4x/8x; the Oversampler itself supports 16x)`;
- `Buffers and filters grow automatically — no \`prepare()\` call needed. \`ensureCapacity()\` is called inside \`upsample\`/\`downsample\`.` → `Buffers and filters grow on demand (\`ensureCapacity()\` inside \`upsample\`/\`downsample\`), but Sand calls \`prepare()\` from \`OnReset\` (\`prepareProcessBuffers\`) for the max block, so the audio thread never grows them.`

- `Latency is measured once per factor via impulse response (6/7/8/8 samples for 2x/4x/8x/16x). Reported to host via \`SetLatency()\`.` → `Latency per factor comes from \`Oversampler::latencyForFactor\` (6/7/8/8 for 2x/4x/8x/16x), reported via \`SetLatency()\` from \`OnReset\`.` (review R8)

Architecture Notes, after "All DSP memory (delay line buffers) is inline …":
> Scratch `AudioBuffer`s (module temp buffers, dry copy, `_osBuffer`) are heap-backed but reserved in `Sand::OnReset` (`prepareProcessBuffers`) for the host's max block × the highest oversampling factor; `process()` must not grow them (it only does for a block longer than the host announced).

Optional: the Grind "Parameter" bullet says `grind ∈ [−1, 1]`; the host parameter is 0–1 (`Sand.cpp:179`), so negative tilt is reachable only from code.

#### Where everything is (nothing pushed)

| Repo | Branch (from) | Worktree | Commits |
|---|---|---|---|
| Sand | `fix/t026-followups` (master 54be993) | `hvoya/Sand-t026` | `9c2dad2` N1 · `cc81d9d` N3 |
| iPlug2 | `fix/host-info-beat` (hvoya 228f1ea8e) | `IPlug2-T-026` | `b36d0d545` T21-5 (proposal: merge only after the user's OK) |
| iPlug2 | `linux/T-026` (hvoya-linux ab632ec4e) | `IPlug2-T-026-log` | this file, `CROSS_PLATFORM_FINDINGS.md` rows, `evidence/t026/` |

Behaviour changes on mac/win: N1 — none audible (bit-identical), no allocation in the audio path, ≈ 1 MB more held per instance at a 1024 max block; N3 — the Windows AAX project compiles Sand's real sources (not built). Proposals change nothing until accepted.

Throwaway, safe to delete after the merge: `hvoya/Sand-t026-{base,linux,pre,diag}` + branches `tmp/t026-linux`, `tmp/t026-diag`; build dirs `linux-research/build/t026-*`. Stash snapshot: "T-026: before restoring regenerated main.rc_mac_dlg (mac branch build)".

Next: independent review (AGENTS §4).


### Review (round 1, 2026-10-17) — verdict: accept with minor fixes (no blockers)

Independent reviewer (AGENTS §4), fresh context. Reviewed: Sand `master..fix/t026-followups` (22d920c N1, e026630 N3), iPlug2 `hvoya..fix/host-info-beat` (6dc1564d3 T21-5), `hvoya-linux...linux/T-026` (586b0a940: this file, `CROSS_PLATFORM_FINDINGS.md` rows, `evidence/t026/`). Nothing in product code or the worker's commits was changed.

**Scope checks.** Main checkouts untouched: `hvoya/Sand` status = only the untracked `Media/` at 54be993; `IPlug2` clean at hvoya 228f1ea8e. `Sand-t026` clean. Shared code touched only on the T21-5 proposal branch (one file, `host_info_model.hpp`). N1 is 3 files / +30 −4, no unrelated refactor. Commit messages state mac/win behaviour per item. Nothing pushed.

**N1 is correct.** Every audio-path buffer either already lived in a member or is now reserved in `OnReset`. The AudioBuffer semantics hold:
- `reserve` only raises the vector's capacity.
- `resize`/`setNumFrames` within that capacity does not reallocate.
- `_tempBuffers`/`_dryHpBufs` are set to 1 channel in the constructor, so `reserve (1, …)` is enough.
- `_dryBuf` grows only upward, so the stride of the `subBuffer (0, numFrames, numChans)` view is always ≥ numFrames.
- `numChans` is clamped to `MAX_NUM_CHAN` in `ProcessBlock`.
- The oversampler's `prepare (maxFrames, 2)` sizes every stage's scratch for 16x, so it covers every factor.
- `GetBlockSize() == 0` only degrades to the old grow-on-demand behaviour.

The audio is identical: the dry copy holds the same values as before.

#### Findings

**R1 — minor — commit 22d920c message and Log N1 "Fix" paragraph (line 99): "a host sending a block longer than its announced max still works (one allocation then)" is true for Sand only.**
- With 32-bit I/O, iPlug core overflows first: `IPlugProcessor::AttachBuffers` copies into the per-channel scratch sized by the block size (`IPlug/IPlugProcessor.cpp:485`, `CastCopy`).
- Reproduced with my copy of the renderer that announces a smaller max (`announce=64`, blocks up to 2048, f32):
  - ASan reports a heap-buffer-overflow in `CastCopy`.
  - Release dies with glibc "corrupted size" / "double free", **before and after N1 alike**.
- With f64 I/O it works as claimed: N1 allocates only on the 2 oversized blocks (16 each), base and N1 renders are bit-identical, and ASan is clean (`build/t026-review-extra/over*.txt`).
- Fix:
  - Reword to "Sand's own buffers still grow on demand for such a block (iPlug's 32-bit scratch does not)".
  - Add a low-severity `CROSS_PLATFORM_FINDINGS` row for iPlug core: a host that exceeds its announced max block with f32 I/O causes a heap overflow in AttachBuffers. VST3/CLAP hosts are not supposed to do this; AU rejects it.
  - Report it, don't fix it.

**R2 — minor — Log N1: the thread-safety argument for calling `prepareProcessBuffers()` from `OnReset` is missing.**
- `OnReset` does not always run in a quiet setup call:
  - CLAP `reset()` runs on the audio thread (`IPlugCLAP.h:107`).
  - AAX calls `SetBlockSize`+`OnReset` inside the render callback (`IPlugAAX.cpp:290-292`).
  - AU calls it from `DoReset` and from `kAudioUnitProperty_BypassEffect` (`IPlugAU.cpp:1237, 2448`). Hosts make those calls on a non-render thread, possibly while rendering.
- The change is still safe:
  - `reserve` can reallocate only when `GetBlockSize()` grew.
  - Outside AAX, the block size grows only in calls that come while the plug-in is not processing (VST3 `setupProcessing`, CLAP `activate`, AU `MaximumFramesPerSlice`).
  - In AAX, the growth happens on the audio thread itself.
- Residue: in the AU bypass/reset path, `AudioBuffer::reserve` still rewrites `_channels` with identical pointers (and `_dataView` in Debug). That is formally a data race, though harmless.
- A bigger race already existed there: the `updateOversampling` in the same `OnReset` resets the module and changes its rate (grind storage/lengths) while `ProcessBlock` may run.
- Fix:
  - Add these 3–4 lines to the Log.
  - Optional, Sand-side: skip `prepareProcessBuffers()` when `GetBlockSize()` ≤ the size already prepared (a `_preparedMaxFrames` member), so audio-thread and AU-bypass resets don't touch the buffers at all.
  - Add a `CROSS_PLATFORM_FINDINGS` row (report only) for the existing AU `BypassEffect`/`Reset` → `OnReset` → `updateOversampling` race with `ProcessBlock`.

**R3 — minor — Log N1 audit table: not complete, and the proof's coverage is not stated.**
- (a) The normal MIDI CC path has no row and is not exercised by the counter (`render-t026.cpp` sends no MIDI). The path is `Sand::ProcessMidiMsg` → `CCMediator::ProcessMidiCC` (`mediator.hpp:127`) → `Mapper::processMidiCC` (`mapper.hpp:97`: template callback, lock-free hazard-pointer read of the published `RTTable`) → `OnParamChange` → atomics. On reading, it does not allocate.
- (b) The Debug-only plog sites on the audio path miss:
  - `dmt_grind.hpp:148-153`: `DmtGrind::reset` logs 2 + 1 per line, per grinder. It runs from `SandModule::reset` on every oversampling change on the audio thread.
  - `svf_zdf.hpp:263`: `checkSubnormal`, assert path.
- (c) The measurement covers the CLAP wrapper only. The VST3/AU/AAX wrappers' own per-block code is not measured.
- Fix: add a row for (a) ("by reading: none; not measured"), add (b) to the plog row, and add one sentence for (c).

**R4 — minor — T21-5 test `evidence/t026/t21-5-beat-table.cpp:26-29`: the oracle is the fix's formula itself.**
- The test runs the real header through a mock plug (`HostInfoModel::updateAll` → `getInfo`), so it proves that the header implements that formula and that the old formula disagreed in 265/432 cases.
- But the "expected" values are not independent of the fix, and these cases are missing:
  - negative PPQ (count-in);
  - the fraction;
  - 7/8 printed rows.
- I checked these against hand-written expectations (`scratchpad/review/t215/neg.cpp`, Apple clang), at 97 BPM:

  | Case | Result |
  |---|---|
  | 4/4 ppq −1, −0.5, −4.25 | beat 4 with fraction 0/50/75 — correct |
  | 3/4 ppq −1; 6/8 ppq −0.5; 7/8 ppq 3.5, 3.75, −0.25 | correct |
  | 4/4 ppq 1.25; 6/8 ppq 0.75; 2/2 ppq 3; 5/4 ppq 4.5 | correct |

  The root cause and the formula are right, including negative PPQ through the floor-based modulo, fractional beats, 6/8 and 7/8.
- Fix: replace `expectedBeat` with a hand-written table {num, den, ppq, beat, fraction} that includes negative PPQ, and check the fraction too.

**R5 — nit — `host_info_model.hpp:189-191`: for a PPQ a hair below 0 (|beats| ≲ num·1e-16), `beats - num*floor(beats/num)` rounds to exactly `num` and the beat shows `num + 1`.**
- Reproduced: 4/4 at ppq −1e-17 shows beat 5. At −1e-9 it shows beat 4, fraction 99, which is correct.
- Display only, and a host must hand over such a PPQ.
- Fix: `if (beatInBar >= num) beatInBar = 0;`, or clamp `i.beat` to `num`. Add that case to the R4 table.

**R6 — nit — Log F2 (line 140): "0 of 35 were stale: a newer CC in that block is applied after the echo (params before MIDI), so the block always ends on the CC value" says more than the data shows.**
- `f2-stepback.py` (re-run) prints `stale: 0; overridden by a newer CC in the same block: 0`. Every echo already equalled the latest CC, so the same-block-ordering mechanism was never exercised.
- Also worth stating: iPlug's VST3 wrapper takes only the last point of each parameter queue per block (`IPlugVST3_ProcessorBase.cpp:297`). MIDI CC arrives through IMidiMapping params, so VST3 applies at most one CC per block. That is why the logs show 71 CC applications in VST3 against 381 in CLAP for the same item. It limits what the VST3 fast-sweep run can show.
- Fix:
  - Reword to "all 35 echoes carried the latest CC value, so no step-back was possible; the same-block ordering was never needed".
  - Add the one-CC-per-block note.

**R7 — nit — `evidence/t026/mac-branch/renders.txt`: the mac lines print `allocs=0` for master too.**
- The malloc counter is Linux-only and compiled out on mac, so these zeros mean nothing. A reader could take them as a mac allocation proof.
- Fix: add a header line to the file (or a word in the Log) saying "counter inactive on mac; only the hashes count".

**R8 — nit — item 7 proposal: the Oversampling section has one more stale line.**
- "Latency is measured once per factor via impulse response (6/7/8/8 …)" (`CLAUDE.md:70`) is out of date: the latency now comes from the `constexpr latencyForFactor` table (`oversampler.hpp:131`), and Sand does not use `measure_latency.hpp`.
- The rest of the proposed text matches the code:
  - 5 bands × 4 lines, lengths 53–887;
  - cross-band injection, LFO on the first and last lines, `sum / numBands * _grindGain`;
  - `srScale = sr*0.6/10000`, `maxSR = 192000*16`, `designRate()` for line lengths and LFO depth;
  - the `grind` parameter range is 0–1 (`Sand.cpp:179`).
- Fix: add "Latency per factor comes from `Oversampler::latencyForFactor` (6/7/8/8 for 2x/4x/8x/16x), reported via `SetLatency()` from `OnReset`."

**R9 — nit (process) — a Docker container named `t026` (iplug2-linux:24.04, Xvfb) was still running at review time.** It was started about 20 minutes before the review began and is not mine. AGENTS §4.5: remove your containers at handover. Fix: `docker rm -f t026` once it is no longer needed.

**Checked and fine:**
- **N3:**
  - Diffed with a script against `Sand-vst3.vcxproj`. The only differences are the VST3-format sources/headers (vst3 side) and the AAX-format ones (aax side).
  - The project and `.filters` list identical ClCompile/ClInclude sets, and every filter used is defined.
  - BOM and LF are preserved on both files; both parse as XML.
  - The diff contains only item-list lines, the removal of `Sand.h`, and the new "hvoya" filter. No setting changed.
  - `proposals/n3-aax-settings.diff` passes `git apply --check` on the branch and mirrors Sand-vst3 (`stdcpp23`/`stdc17`, the PreBuildEvent with "AAX").
- **Audit-table line references** (`Sand.cpp:95/105/108-109/117/333`, `sand_module.hpp:170/187-188/194/317/914/1083`, master `:304`, hvoya `host_info_model.hpp:58/71/81`, `audio_buffer.cpp:19`, `mapper.cpp:128/230`, `oversampler.hpp:163/202`, `IPlugAAX.cpp:291`) all point at the code described.
- `PLOG_DISABLE_LOGGING` is set in Release in `Sand-mac.xcconfig:29`, `Sand-win.props:8` and `hvoya.cmake:61`.
- The CPU and mac render evidence matches the Log: 46/46 mac hashes are equal; the CPU spreads are as stated.
- The commit messages for N1, N3 and T21-5 explain the mac/win behaviour, and the Log states honestly what was not done: no Windows/AAX build; mac F2, auval, Bitwig and Live left to the user's checklist.

#### Re-run by the reviewer (new output dirs only)

- **N1 allocations, Linux Docker, the worker's builds, `run-allocs.sh`:**
  - after (`build/t026-review-allocs-after`): **47/47 configurations with `allocs=0` in total, first block included**;
  - before (`build/t026-review-allocs-before`): 1 per block, 316 958 blocks, plus 12–24 on the first block and 13 per os change;
  - **`render.sha256` before = after (47/47)**, and equal to the worker's `allocs-after/render.sha256`; non-finite 0.
  - I checked that the n1 build's source tree (`Sand-t026-linux`) matches `fix/t026-followups` in `Sand.cpp`/`Sand.hpp`/`sand_module.hpp`, and that the base build is `Sand-t026-base`.
- **Extra N1 cases** (`build/t026-review-extra/extra.txt`):

  | Case | Result |
  |---|---|
  | Announced max 1 | 0 allocations |
  | os swept every block at max 256 | 0 allocations |
  | Both, ASan | clean |
  | Both, base vs N1 renders | bit-identical |

  Oversized blocks: see R1.
- **N4 stress, quick** (`run-stress.sh … build/t026-review-stress quick`): **160 Release + 160 ASan renders, 0 crashes, 0 ASan reports, 0 non-finite** (rates 44.1–192 kHz × os 1–8x × filter/mix sweeps × 0-frame blocks with valid/null pointers), as the Log claims.
- **T21-5:**
  - built with Apple clang into the scratchpad from the line in the test's header comment (plus `-I Sand-t026`);
  - result: "old formula wrong in 265, header wrong in 0", rc 0, the same as `t21-5-apple-clang.txt`;
  - my additional hand-written cases: R4/R5.
- **F2:**
  - opened `f2-sweep-vst3/f01-end.png`: REAPER 7.82, VST3 Sand open, comp knob at 0.000 at the end, transport stopped;
  - `run.log` has `host-value compression amnt 0.000 env-points 17`;
  - `f2-stepback.py`: VST3 has 35 echoes, 0 stale, 0 step-backs; CLAP has 0 echoes. See R6 for the wording.

Required before handover: R1–R4 (text and test only; no product-code change is required). R5–R9 are optional.

### 2026-10-18 — fixes after review round 1

All of R1–R9 taken (the earlier sections above now carry the corrected text and hashes):
- **R1:** N1 commit message and Log reworded ("Sand's own buffers still grow on demand …; iPlug's 32-bit scratch does not"); `CROSS_PLATFORM_FINDINGS` row **T26-b** (iPlug core, report only).
- **R2:** thread-safety note added under N1. Code: `Sand::prepareProcessBuffers` returns unless the max block grew (`_preparedMaxFrames`), so audio-thread / AU-bypass `OnReset` calls no longer touch the buffers. Row **T26-c** for the older AU `OnReset` → `updateOversampling` race (report only).
- **R3:** audit table: MIDI CC row (by reading, not measured), `dmt_grind.hpp:148-153` and `svf_zdf.hpp:263` added to the plog row, CLAP-only coverage stated.
- **R4:** the T21-5 test now checks the header against 36 hand-written cases (count-in, fraction, 6/8, 7/8, 5/4, 2/2) × 6 tempos: 168/216 wrong before, 0 after, Apple clang and GCC 13.
- **R5:** `host_info_model.hpp`: a PPQ a hair below 0 shows the bar start instead of beat num+1 (a case in the table).
- **R6:** F2 wording: all echoes carried the latest CC; the one-CC-per-block limit of iPlug's VST3 path noted.
- **R7:** note at the top of `mac-branch/renders.txt` and in the Log: the counter is Linux-only.
- **R8:** the stale latency line added to the CLAUDE.md proposal.
- **R9:** container `t026` removed.

New hashes (branches rewritten before any merge; nothing was pushed or merged):
- Sand `fix/t026-followups`: `9a89783` N1 (was 22d920c) · `aacc040` N3 (was e026630, unchanged content).
- iPlug2 `fix/host-info-beat`: `b36d0d545` T21-5 (was 6dc1564d3).

Re-run on the new N1 (`evidence/t026/round1/`):
- Linux Release: `run-allocs.sh` **47/47 configurations with 0 allocations**, `render.sha256` = before (47/47, incl. the re-activation cases where the max block grows and `prepare` runs again); VST3 validator ×20: 20 × 537/537.
- mac (rsync copy, against `IPlug2-T-026` now on `fix/host-info-beat`, so Sand also compiles with the T21-5 change): CLAP Release + VST3 Debug build; 46/46 render pairs bit-identical to master; VST3 Debug ×20: 0 failures; clap-validator 38/0.

Next: review round 2.

### Review (round 2, 2026-10-18) — verdict: pass (review-done); two optional nits

Same reviewer, round 2 of 2. Reviewed: Sand `master..fix/t026-followups` = `9a89783` N1 + `aacc040` N3; iPlug2 `hvoya..fix/host-info-beat` = `b36d0d545` (single commit on 228f1ea8e); `linux/T-026` `0ef90e21d`. Main checkouts are still untouched: `hvoya/Sand` 54be993 with only `?? Media/`; `IPlug2` 228f1ea8e, clean. `Sand-t026` and `Sand-t026-linux` are clean. No container is left running; the worker's `t026` is gone and I removed mine (`--rm`).

**R1–R9 fixes, each checked:**
- **Rewrite = content.** `git patch-id` of `Sand.cpp`+`sand_module.hpp` is identical for 22d920c and 9a89783. The only N1 delta is the `Sand.hpp` guard (+6/−1). `aacc040` has the same patch-id and message as e026630.
- **R1:** the commit message and Log now say "Sand's own buffers still grow on demand … iPlug's 32-bit scratch does not". Row T26-b is accurate (iPlug core, report only).
- **R2 — the `_preparedMaxFrames` guard (`Sand.hpp:180-191`) is correct:**
  - `AudioBuffer::reserve` and `Oversampler::prepare` never shrink, and nothing in Sand drops capacity: there is no move-assign or rebuild of `_osBuffer`, the module buffers or the oversampler.
  - So skipping preparation when `GetBlockSize()` ≤ the prepared size loses nothing.
  - `GetBlockSize() == 0` returns early and leaves the old grow-on-demand behaviour; a later larger block size prepares normally.
  - A sample-rate change needs no re-preparation: the buffer sizes depend only on frames × 8.
  - The thread-safety note and row T26-c are accurate.
- **R3:** the audit table now has the MIDI CC row, the two Debug plog sites and the CLAP-only coverage sentence.
- **R4:** the test now checks the real header against 36 hand-written cases, beat and fraction, × 6 tempos.
- **R5:** `if (beatInBar >= num) beatInBar = 0;` makes ppq −1e-17 show 1.00, the bar start. This is a reasonable reading of "≈ 0". At −1e-9 it still shows 4.99, checked with my round-1 `neg.cpp`.
- **R6, R7, R8:** the wording, the header note in `mac-branch/renders.txt` and the latency line in the CLAUDE.md proposal are all as asked.
- **R9:** done.

**Re-run by the reviewer** (new dirs `build/t026-review2-*`):

| Check | Build / dir | Result |
|---|---|---|
| `run-allocs.sh` | `t026-r1-rel` → `build/t026-review2-allocs` (I checked this build is the tree `Sand-t026-linux` = a027e6d + merge of 9a89783/aacc040, Release) | **47/47 configurations with 0 allocations**; `render.sha256` = base (47/47) = the worker's `round1/linux/render.sha256` |
| Guard regression, Release, malloc counter + bit-exact vs base | `build/t026-review2-extra/guard.txt` | See the case table below |
| Fresh ASan CLAP build | `build/t026-review2-asan` (`sand-build.sh … asan`) | `guard-asan.txt`: the shrink, same-sr and grow cases, os swept every block with null-pointer 0-frame blocks, filter sweep at 192 kHz os 8x, and the f64 announce case — **0 ASan reports, 0 non-finite** |
| T21-5, Apple clang, new test | branch header | "header's beat or fraction wrong in 0", rc 0 |
| T21-5, Apple clang, new test | old 228f1ea8e header (`git archive` into the scratchpad) | **168**, as in `t21-5-apple-clang.txt` / `t21-5-gcc.txt` |
| mac round-1 evidence | `round1/mac` | 46/46 hashes before = after; VST3 Debug ×20 clean; clap-validator 38/0 |

Guard-regression cases (Release, malloc counter, bit-exact vs base):

| Case | Base | r1 | Renders |
|---|---|---|---|
| Re-activation with a **smaller** max block at another rate (1024 → 1024/128,64 at 96 kHz) | allocates | **0** | bit-identical |
| Re-activation at **another rate, same** max block (44.1 → 96 kHz, 512) | allocates | **0** | bit-identical |
| Re-activation with a **larger** max block (64 → 4096) | allocates | **0** | bit-identical |
| Announced max 1 | allocates | **0** | bit-identical |
| Host exceeding its announced max (f64) | — | 16 allocations on each of the 2 oversized blocks only | bit-identical |

The 0 for the larger max block shows that `prepare` runs again when the max block grows.

**Nits (optional, cosmetic; no re-review needed):**
- **R10 — nit — `Sand.hpp:182-183` comment:** "Some OnReset calls race ProcessBlock (CLAP reset, AU bypass/reset, AAX from the render call)" is inaccurate for CLAP and AAX. CLAP `reset()` and the AAX render-call `OnReset` run *on* the audio thread, not beside it; only the AU calls can race. Suggested: "Some OnReset calls run on the audio thread (CLAP reset, AAX render call) or beside it (AU bypass/reset), so the buffers are touched only when the block grows." The commit message's parentheticals already say this correctly.
- **R11 — nit — `evidence/t026/t21-5-beat-table.cpp:6`:** "Exit status 1 if the header disagrees with the expected beat" should now read "beat or fraction".

Task-file housekeeping for the worker/coordinator: the **Status** line (line 3) still says "independent review pending". Set it to `review-done`.

### 2026-10-18 — after review round 2: hand-over

- **R10** fixed: the `Sand.hpp` comment now says CLAP reset / the AAX render call run *on* the audio thread and AU bypass/reset *beside* it. Comment only; N1 rewritten once more before any merge: Sand `fix/t026-followups` = **`9c2dad2` N1 · `cc81d9d` N3** (9a89783/aacc040 superseded; code identical except that comment).
- **R11** fixed: the test's header says "beat or fraction".
- Final branches (nothing pushed or merged):
  - Sand `fix/t026-followups` (from master 54be993), worktree `hvoya/Sand-t026`: `9c2dad2` N1, `cc81d9d` N3 — mergeable at the user's command.
  - iPlug2 `fix/host-info-beat` (from hvoya 228f1ea8e), worktree `IPlug2-T-026`: `b36d0d545` T21-5 — **proposal, merge only after the user's OK**.
  - iPlug2 `linux/T-026` (from hvoya-linux ab632ec4e), worktree `IPlug2-T-026-log`: this file, findings rows, `evidence/t026/`.
- Waiting on the user: T21-5 merge; the AAX settings patch (`proposals/n3-aax-settings.diff`); the CLAUDE.md text (item 7); the FL check (N4 checklist); the mac host checklist (F2) or permission to install the AU into `~/Library` for `auval`/REAPER AU; BACKLOG wording (N4).
- Cleanup: Docker containers removed; lock released. Throwaway worktrees/branches/build dirs listed in "Where everything is" can go after the merge; worktrees left in place for the coordinator. `IPlug2-T-026` and `IPlug2-T-026-log` have the locally modified `Dependencies/IPlug/VST3_SDK/README.md` from `install-sdks.sh` — never commit it.
