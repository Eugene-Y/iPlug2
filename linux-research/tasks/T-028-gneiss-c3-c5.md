# T-028 — Gneiss: don't write the host's input (C3), texts parse back (C5), tail / sleep status

- **Status:** in review (user, 2026-10-18: "надо фиксить")
- **Owner:** T-028 chat (2026-10-18), lock `.linux-port-locks/T-028`
- **Branches/worktrees:** Gneiss `fix/t028-input-text-tail` from `master`, in `/Volumes/Storage/code/hvoya/Gneiss-t028`. Not Linux work: no iPlug2 branch unless a shared change turns out to be needed (then it is a proposal for the user first). Task Log and evidence: iPlug2 `linux/T-028` from `hvoya-linux` in `/Volumes/Storage/code/IPlug2-T-028-log` (as T-026 did).
- **Commits:** the user allows commits on `fix/t028-input-text-tail` (2026-10-18), which overrides Gneiss `CLAUDE.md`'s no-commit rule for this branch only. Never touch `master` or the main checkout; never push.
- **Depends on:** nothing.
- **Start:** read AGENTS.md (whole), then this file, then claim it with the lock (AGENTS §5).
- **Conflicts:** `Gneiss.cpp` (`ProcessBlock`/`processChunk`, `initializeParams`), `Gneiss.hpp`. Takes Gneiss's part of T-023 (C3, C5); T-023 keeps whatever clap-validator still finds after this.

## Read first
Gneiss `CLAUDE.md`, `ARCHITECTURE.md`, `docs/fast_clean_code.md` (preallocation, no allocation on the audio thread), `docs/serialization.md` (state must not change). AGENTS §2 (product rules), §4 (independent reviewer). `CROSS_PLATFORM_FINDINGS.md` C3, C5. Sand did the same fixes in T-024 (F4, F5: Sand `00cbc41`, `e9ea624`) — reuse the approach.

## Items

### 1. C3 — the host's input buffer is never written
`processChunk` (`Gneiss.cpp:~690–940`) works in `input`, a view of the host's `ins`: at OS 1x the filter runs in place on it, at OS >1x `downsample` writes into it, then `input *= mix`, `input += _dryBuf`, `output.fillFrom (input)`. With 64-bit I/O (VST3 64-bit, CLAP data64, VST2 double) iPlug passes the host's own buffers, so an out-of-place host gets its input overwritten (clap-validator `process-audio-double-in-place` / `-out-of-place`).

What exists already: `ProcessBlock` cuts host blocks into chunks of at most `kMaxChunkFrames` (512), and `prepareAudioBuffers()` — called from the constructor (`Gneiss.cpp:189`) — reserves `_dryBuf` (the input copy for the dry mix), `_osBuffer`, `_mixWorkBuf` for `MaxNChannels (kInput)` × that size. The user's requirement: memory for any copy of the input is allocated there, in the constructor, sized by `kMaxChunkFrames`; nothing allocates on the audio thread.

Options, recommended first:
- **A. Process in `output`** (Sand F5): `_dryBuf.fillFrom (input)` first, then `output.fillFrom (input)` (a no-op for in-place hosts, where `ins == outs`), and run the rest of the chain on `output`. No new buffer, still one copy per chunk, and the final `output.fillFrom (input)` goes away.
- **B. A preallocated wet buffer** reserved in `prepareAudioBuffers` next to `_dryBuf`. Costs one more copy and `chans × 512` doubles; take it only if A turns out unsafe (e.g. an output with fewer channels than the chain needs).

Either way: grep the chain for every other write through `input` (oversampler, note/model code that gets the buffer), and keep the channel-count handling (`getMinChans`) as it is.

### 2. C5 — every parameter text parses back to the value it shows
The user types the value as Gneiss displays it, or the host converts text → value (CLAP `text_to_value`, AU `ParameterValueFromString`, VST3 `getParamValueByString`, IGraphics text entry). `IParam::StringToValue` is `atof` unless the param has a `TextToValueFunc` (hvoya 228f2efc2). Today "1.06 kHz" → 1.06 Hz, "50%" → clipped to 1, "H 12 ms" → 0 (clap-validator `param-conversions`: `'1.06 kHz' -> 0.0416 -> '1.1 Hz'`).

- `hzDisplay` (`Gneiss.cpp:989`; main hz, preFlt A/B hz): pair it with `hvoya::params::makeSegmentedParser` built from **the same segments** — hoist the segment list into one constant used by both, so display and parser cannot drift.
- Every other `SetDisplayFunc` / `makeDisplayFunc` (`Gneiss.cpp:1012–1213`: gain dB, gain on dry `%`, damping hold `H … ms` / release `R … ms|s` / thresh dB, sat type `A>B 30%`, glide ms, …): give each a parser that accepts exactly what its display prints (prefix, unit, segment) and also a bare number in the param's own unit. Enumerate them from the code, not from this list.
- Write a table test over every parameter and a grid of values: value → text → value → text gives the same text (that is what clap-validator checks), and the second value is within the display's resolution of the first.

**What this does not touch (the user asked, 2026-10-18):** `StringToValue` / `TextToValueFunc` are called only on a text → value conversion. Automation, state and preset load, knob drags, morph, MIDI CC all pass numbers and never go through text, so a precisely set cutoff is never moved by this change. A typed or host-converted text gives the value the text shows, to the display's precision ("1.06 kHz" → 1060 Hz) — that is the nature of typing a value, not a loss in the parser. Verify it anyway: renders and saved state bit-identical before/after for a project with automated and morphed cutoffs.

### 3. C5 — tail and `process-sleep-process-status`
Gneiss declares no tail. clap-validator `process-sleep-process-status` failed in the T-026 merge smoke and passed in T-024's: find out why it is intermittent, then declare a tail that matches the filter (a resonant ladder rings; infinite while self-oscillating?) through iPlug's tail API, and check how iPlug's CLAP/VST3/AU wrappers turn it into the process status. A tail changes how hosts on mac/win treat Gneiss after the input stops (they keep processing it longer): write the behaviour change down for the user's decision before merging.

## Testing
- **Audio:** renders bit-identical to `master` — 32- and 64-bit I/O, in-place and out-of-place, OS 1x/2x/4x/8x, block sizes 1/64/512/513/4096, mix/gain automation. 64-bit out-of-place: the host's input unchanged after `process`. Reuse T-024's offline CLAP host `evidence/t024/render-clap.cpp` (writes the output, counts changed input samples and non-finite samples, times `process()`) with its drivers `evidence/t024/run-checks.sh` (Linux, Docker) and `mac-checks.sh`, and `cpu-bench.sh` for CPU before/after; T-024's Log shows how they were run and compared (`render.sha256`).
- **No allocation in `process()`**: the T-026 N1 malloc counter (`evidence/t026/run-allocs.sh`) reports 0 for every configuration.
- **Validators:** clap-validator on Linux and mac — `process-audio-double-*`, `param-conversions`, `process-sleep-process-status` pass; remove those lines from `linux-research/scripts/known-clap-failures/Gneiss.txt`. VST3 validator ×10, no crash.
- **Hosts:** REAPER `scripts/steps/gneiss.txt` CLAP + VST3 in Docker; on mac, text entry of a cutoff in REAPER's parameter window ("1.06 kHz" sets 1060 Hz).
- **mac build** of CLAP/VST3/AUv2/APP from a temporary detached worktree (`scripts/coord/mac-xcodebuild.sh`); AU through `auval` only with the user's OK to install into `~/Library`.

## Acceptance
- No write to the host's input; no allocation on the audio thread; input copies preallocated in the constructor.
- Every parameter's display text parses back (table test + clap-validator).
- Tail declared or the reason not to written down; mac/win behaviour change listed for the user.
- Audio and state bit-identical to `master` except where the tail intentionally changes host behaviour.
- Independent reviewer passed (AGENTS §4).

## Log

### 2026-10-18 — start

Lock `.linux-port-locks/T-028` taken. Worktrees:
- `hvoya/Gneiss-t028`, branch `fix/t028-input-text-tail` from master 4dc02eb (the work).
- `IPlug2-T-028-log`, branch `linux/T-028` from `hvoya-linux` f88be6b11 (this Log, evidence).
- Throwaway, never merged: `hvoya/Gneiss-t028-base` (detached master, mac "before"), `hvoya/Gneiss-t028-base-linux` (detached `linux-experiment` 8a0f05b, Linux "before").
- Linux builds use `-DIPLUG2_DIR=/Volumes/Storage/code/IPlug2-linux` (mounted read-only; this worktree has no SDKs).

### 2026-10-18 (2) — C3, C5 committed; Linux evidence; tail cause found

**Commits on Gneiss `fix/t028-input-text-tail`:**
- `b600156` **C3** — `processChunk` takes `const AudioBuffer& input`: at OS 1x `output.fillFrom (input)` and the chain runs in `output`; at OS >1x upsample from `input`, downsample into `output`. The final `output.fillFrom (input)` is gone (one copy less at OS >1x, the same at 1x). No new buffer: `_dryBuf` (preallocated in `prepareAudioBuffers`) is the only input copy, as before. Nothing else in the chain wrote through `input` (`_model.processBuffer` gets `work`, the oversampler reads `input`).
- `3e0cb40` **C5** — new `param_text.hpp`: each display and its parser built from one description (`SegmentedText` = prefix + hvoya segments + off text; `setPercentText`; `setRingBlendText`). Used by main/preA/preB hz, gain, gain on dry, damping hold/release/thresh, sat type, the 3 glide times — every `SetDisplayFunc` in `Gneiss.cpp`. Bare numbers are read in the display's base unit (Hz, dB, ms, %; sat type: the position). "nan" reads as 0.
  - The header is not added to the Xcode/VS projects (header-only, found by the include path; the projects are not touched).

**Harness** (`evidence/t028/`):
- `render-t028.cpp` = T-026's `render-t026.cpp` + `load=` (load a state before activating), `state=` (save after), `silence=`, process-status counts. **A fresh Gneiss instance is random** (`applyRandomStarterPreset`, `seedFreshInstancePoints`): two fresh instances of the same build render differently, so every render loads `ref-morph.state` (a saved fresh instance: starter preset + morph points). With it, base = base and base = after are deterministic.
- `run-renders.sh`: 54 configurations — OS 1x/2x/4x/8x × {f32/f64, in-place/out-of-place, blocks 1/64/512/513/4096, mix+gain automation, cutoff+res automation with morph off, morph X/Y automation, random blocks with 0-frame blocks}, plus OS switched every block and a re-activation 48k → 96k.
- `param-text.cpp`: table test through the CLAP params extension. Per param: every step (stepped) or a 4001 grid + 4000 random values + the values at each text change found by bisection (±2 ulps); value → text → value → text must give the same text (FAIL), the parsed value must stay in the shown value's display bin (JUMP); "nan" must parse finite; 46 typed cases ("1.06 kHz", "50", "R inf", "ASINH>TASSYM 30%", …) must show the expected text. `dump=` writes the texts for a build-independent value set, to diff two builds' displays.

**Linux results** (Docker 24.04, Release, `-DIPLUG2_DIR=IPlug2-linux`; base = `linux-experiment` 8a0f05b, after = `tmp/t028-linux` = branch + `linux-experiment`):
- Renders: **54/54 raw outputs and 54/54 saved states bit-identical** (`linux-{base,after}/render.sha256`, `state.sha256`). Input samples changed (out-of-place): **5 568 000 → 0**. Allocations inside `process()`: **0 → 0**. Non-finite: 0.
- Param texts (`param-text-linux-{base,after}.txt`): base **82 739 FAIL** in 14 params, 53 params where "nan" parses to NaN, 25/46 typed cases wrong; after **16 FAIL** (all `'-0.000' -> '0.000'` in `preFlt band mode` and `pt tilt`, iPlug's default display, see findings), 13 JUMP (sat type at the ring's top, "TANH" = 3 → 0: same sound, expected), 41 params where "nan" parses to NaN (all iPlug's default `atof` path), **0/46 typed cases wrong**.
- Display texts that changed (`display-text-diff-linux.txt`, 8001 values per param): only where the old text did not read back — sat type at 100% ("TANH>ASINH 100%" → "ASINH"), damping release 999.5–1000 ms ("R 1000 ms" → "R 1.00 s") and ≥ 1999.995 ms ("R 2.00 s" → "R inf"), damping hold 9.95–10 ms ("H 10.0 ms" → "H 10 ms"), gain / thresh in (−0.05, 0) dB ("-0.0 dB" → "0.0 dB"), glide 999.5–1000 ms ("1000 ms" → "1.00 s"). Not in the sampled set, by construction: exact x.5 ms ties of hold/release/glide now round to even ("42.5" → "R 42 ms", was "R 43 ms").
- clap-validator 0.4.1: base 34 pass / 4 fail (double in/out-of-place, sleep-process-status, param-conversions); after **38 / 0** in this run (`clap-validator-linux-{base,after}.txt`). sleep-process-status is intermittent, see below.

**Tail: why `process-sleep-process-status` is intermittent** (clap-validator 0.4.1 source, `src/tests/plugin_instance/processing.rs`):
- The validator host implements the CLAP `tail` extension, so iPlug returns `CLAP_PROCESS_TAIL` and `tail.get()` = 0. The validator then treats the plug-in as asleep once `tail < quiet_time`, i.e. after the first silent 512-sample block, and fails if a later silent block's output exceeds −60 dBFS (`check_channel_quiet`).
- Gneiss rings after the input stops, and how long depends on the sound: a fresh instance — what the validator tests — loads a **random starter preset**. Measured (`tail-ringout-linux.txt`, ref state, morph off): 76–114 ms above −60 dBFS with damping on (default R 100 ms, also R 2 s, res up to 1); with damping off ("R inf") and res 1 the self-oscillation keeps going (7.7e-4 = −62 dBFS after 19 s). Any ring longer than one block fails the test.
- Real hosts with the same logic: CLAP hosts that implement `tail` (Bitwig, REAPER) may stop processing Gneiss right after its input goes silent and cut the ring / self-oscillation. Hosts without it get `CONTINUE_IF_NOT_QUIET` (they watch the output), which is right for Gneiss.
- iPlug passes the tail on only statically for VST3 (`getTailSamples`) and AU (`TailTime`); only CLAP has a change notification (`SetTailSize` → `tail->changed`). So a tail that follows the damping params would be live in CLAP only.

Next: mac build + validators, REAPER steps, VST3 validator ×10; tail proposal to the user (not implemented yet).

### 2026-10-18 (3) — tail (user's decision), docs, mac + Linux results on the final branch

**User decision (chat, 2026-10-18), asked before implementing:** a *fixed finite* tail (options offered: infinite (recommended) / dynamic from damping / fixed finite, e.g. 15 s / none).

**Commits on Gneiss `fix/t028-input-text-tail`** (on top of `b600156` C3, `3e0cb40` C5):
- `2f84c42` **tail 15 s** — `Gneiss::kTailSeconds = 15`, `declareTail()` = `SetTailSize (lround (15 · GetSampleRate()))` from the constructor and every `OnReset` (samples, so it follows the host rate; OnReset only stores host-facing state, nothing of the audio thread). Why 15 s: with damping on, the gate drains every filter state 60 dB down within ~7 releases, the longest finite release being just under 2 s; every ring measured was under 1 s (`tail-*.txt`: 65–667 ms on 12 fresh instances, ≤ 167 ms at full resonance with R 2 s / gain +24 / OS 8x / low cutoffs). A self-oscillation with damping off ("R inf") never ends: hosts now stop it 15 s after the input goes silent.
- `74552f1` docs — `ARCHITECTURE.md` file map gets `param_text.hpp`; `docs/serialization.md` §3a: custom display texts go through `param_text.hpp`; stale §2 sentence fixed (Gain scales the wet before the sum, not the sum).

**What the tail changes on mac/win** (for the user's merge decision):
- CLAP, host with the `tail` extension (Bitwig, REAPER): `CLAP_PROCESS_TAIL` with 15 s instead of 0 — the host may stop calling Gneiss 15 s after silence instead of at once.
- CLAP, host without it: iPlug's local tail — `CONTINUE` until 15 s of silent input, then `SLEEP` (was `CONTINUE_IF_NOT_QUIET`, where the host watched the output). Measured: first `SLEEP` at frame 768 000 = 1 s of audio + 15 s of silence at 48 kHz; none within 10 s at 96 kHz (`tail-status-linux.txt`).
- VST3 `getTailSamples` 15 s · rate (was 0): hosts that suspend silent plug-ins (Cubase/Nuendo option) keep Gneiss running 15 s.
- AU `TailTime` 15 s (was 0). auval not run (needs an install into `~/Library`, the user's OK).
- Audio and state unchanged (renders below).

**Final results** (branch = `2f84c42`; Linux `tmp/t028-linux` 2684dd5 = branch + `linux-experiment`; mac = Debug xcodebuild of the branch / of master, all paths in the scratchpad, nothing in `~/Library`):

| Check | base (master / linux-experiment) | branch |
|---|---|---|
| Renders, 54 configs, raw + saved state (`{linux,mac}-{base,after}/`) | — | **bit-identical** to base, Linux 54/54 + 54/54, mac 54/54 + 54/54 |
| Host input samples changed (out-of-place, 64-bit) | 5 568 000 | **0** (Linux and mac) |
| Allocations in `process()` (Linux hook) | 0 | **0** |
| Param text table (`param-text-{linux,mac}-*.txt`) | 82 739 FAIL, 25/46 typed wrong | **16 FAIL** (T28-a, iPlug), 13 JUMP (sat ring top), **0/46 typed wrong**; Linux = mac |
| clap-validator Linux ×20 (`clap-validator-linux-x20.txt`) | sleep-process-status failed 20/20 (+ double ×2, param-conversions) | **38 pass / 0 fail ×20** (one run: 1 warning, not reproduced in 15 more; the only warning clap-validator emits is the denormal-slowdown timing one) |
| clap-validator mac ×10 (`mac-validators/`) | 35 / 3 ×10 (param-conversions, double in/out-of-place; sleep passed on mac — why it differs from Linux was not investigated) | **38 / 0 ×10** |
| VST3 validator ×10 (Linux Release, mac Debug) | — | 537 passed, 0 failed, ×10 each |
| REAPER `steps/gneiss.txt` (Docker) | VST3: `g04b-context-menu` FAIL (also in every merge-smoke since pre-T-007) | CLAP **PASS**; VST3 same single FAIL as base (T28-c); screenshots looked at |
| mac build CLAP/VST3/AUv2/APP of the branch | — | all exit 0 |
| CPU, Linux Release, best of 7, 10 s audio (`cpu-linux.txt`) | 0.185 / 0.186 / 0.780 / 1.531 s | 0.185 / 0.185 / 0.772 / 1.533 s (OS 1x f64, 1x f32, 4x f64, 8x f32) |

**Not done here (need the user):**
- REAPER on mac: type "1.06 kHz" into the cutoff in REAPER's parameter window → 1060 Hz. The same CLAP `text_to_value` call is what `param-text` runs on the mac build, but a real REAPER needs Gneiss installed into `~/Library`.
- auval (same reason).

**Cross-platform findings, report only** (also in `CROSS_PLATFORM_FINDINGS.md`): T28-a (iPlug default display "-0.000" doesn't read back), T28-b ("nan" text → NaN param value through `atof`/`strtod`), T28-c (REAPER VST3 context-menu step, harness, pre-existing).

`scripts/known-clap-failures/Gneiss.txt` is now empty: valid once the Gneiss branch is merged into `linux-experiment` (until then `package-smoke-in.sh` of the old Gneiss reports its 4 known failures as new).

Next: independent review.

### Review round 1 (independent reviewer)

**Verdict: changes requested — minor only.** The code is approved: no blocker, no major. Findings M1–M3 are about the wording of commit messages and the Log, which is what the user's merge decision rests on. M4 needs a decision. Round 2 only needs to re-read the reworded texts, unless M4 leads to a code change.

**Reviewed:** Gneiss `master..fix/t028-input-text-tail` (b600156, 3e0cb40, 2f84c42, 74552f1); `tmp/t028-linux` 2684dd5 (= linux-experiment 8a0f05b + fix branch up to 2f84c42; it lacks only the docs commit); IPlug2 `hvoya` (read-only): `IPlugCLAP.cpp` process status/`SetTailSize`/`activate`/`reset`, VST3 `getTailSamples`, AUv2 `TailTime`, AUv3 `tailTime`, VST2 `effGetTailSize`, `IParam::StringToValue`, clap-helpers `canUseTail`/`tailChanged`; hvoya `AudioBuffer::copyContentFrom`, `Oversampler::upsample/downsample`, `makeSegmentedDisplay/Parser`.

**Re-run by me** (own build `build/t028r-after` from `Gneiss-t028-linux` 2684dd5 in Docker, Release; outputs in `build/t028r-*`, not committed):
- Renders, `run-renders.sh` with `ref-morph.state`, base `build/t028-base` vs `t028r-after`: **54/54 render.sha256 and 54/54 state.sha256 identical**. Both files are also byte-identical to the executor's `linux-after/*.sha256`. Input changed: 5 568 000 (base) → **0**. Allocations in `process()`: 0 / 0. Non-finite: 0.
- param-text, Linux: base `FAIL=82739 JUMP=4 nan-fails=53 typed-fails=25`, after `FAIL=16 JUMP=13 nan-fails=41 typed-fails=0`, as logged. On mac (`mac-after` Debug CLAP, binary built 12:40, after 2f84c42): the same after totals.
- clap-validator, Linux after ×6: 38 pass / 0 fail / 0 warnings each time. The four target tests ran and passed. On mac after ×2: 38 / 0.
- VST3 `validator -e`, Linux after ×3: 537 passed / 0 failed, exit 0. Without `-e` it runs 47 tests, so the Log's 537 is the `-e` count; the Log should say so.
- REAPER screenshots I looked at: `reaper-clap/g01-knob.png` shows the UI with real values. `reaper-vst3/g04b-context-menu.png` shows no menu: that is T28-c, pre-existing. `gdb-crash.txt` is only the final SIGINT thread dump, not a crash.

**Checked and OK:**
- C3:
  - `processChunk` takes `const AudioBuffer& input`. The const is enforced by the API: `getChan`, `*=` and `+=` are non-const.
  - Every path from the host buffers was followed: `_dryBuf.fillFrom (input)`, `output.fillFrom (input)` at 1x, `upsample (input, _osBuffer)` at >1x. `_model.processBuffer` gets `work` = `output` or `_osBuffer`; `downsample` writes into `output`; mix, gain and the dry sum operate on `output` and `_dryBuf`. Nothing else in `ProcessBlock` touches `ins`.
  - In-place hosts are safe. `_dryBuf` is copied before anything writes `output`. `copyContentFrom` skips the self-copy (`ins == outs`), and at >1x `upsample` reads `input` before `downsample` writes `output`.
  - No new buffer; `getMinChans` handling is unchanged.
- C5:
  - Every display function in Gneiss (all of them are in `initializeParams`; grep over `*.cpp`/`*.hpp` finds none elsewhere) now goes through `setText`, `setPercentText` or `setRingBlendText`. Each one's display and parser are built from the same `SegmentedText` or names list.
  - Lambda captures are by value. The `string_view`s point at literals or at the `static constexpr` `kDampingTimeOffLabel`, and the sat-type `span` points at the `inline constexpr` `kSatTypeNames`, so nothing dangles.
  - The NUL-termination assumption holds. Every `data()` that reaches `strtod` is a suffix of the original `const char*`: only `trimLeft`/`substr`/`remove_prefix` are applied; `trimRight` is used only for comparisons.
  - NaN → 0 happens before `Constrain`, which would let NaN through.
  - "State and automation never go through text" is confirmed: Gneiss has no `StringToValue`, `atof` or `OnTextEntryCompletion` of its own, and `TextToValueFunc` is reached only from `IParam::StringToValue`. The renders and states above are bit-identical.
- Tail, threads and lifecycle:
  - Constructor: `IPlugCLAP::SetTailSize` → `canUseTail()` is false before `init` (`_hostTail` is null), so nothing is queued.
  - `activate` → `OnReset` → `mTailUpdate = true` → `tailChanged()` from the next `process()`. That is the audio thread, which is what clap-helpers' `ensureAudioThread` requires.
  - CLAP `reset()` (audio thread) → `OnReset` → the same path. Harmless.
  - No allocation: `SetTailSize` only stores an int.
  - VST3 `getTailSamples` = `GetTailSize()`; AUv2/AUv3 report `GetTailSize()/GetSampleRate()` = 15 s at any rate.
  - CLAP without the host tail extension: `localTail` gives `CONTINUE` until 15 s of silent input, then `SLEEP`. That matches `tail-status-linux.txt` (first sleep at frame 768 000).
- Code quality:
  - `param_text.hpp` reads well and is small. The `gneiss::params` namespace matches the existing `gneiss::ui`.
  - Comments carry WHY only.
  - The removed gain-display comment about the morph epsilon (±1e-4) is covered: the segmented display now zeroes `|v| < 0.05`.
  - No stale comments found around the changed code. The C3 and C5 commit messages are accurate, and each says what changes on mac/win.

**Findings**

- **M1 (minor) — the tail commit message `2f84c42` misdescribes CLAP hosts without the tail extension.**
  - It says "hosts without it still decide by the output" and "hosts keep processing Gneiss for 15 s … instead of stopping at once".
  - For those hosts it is the other way round. Before, they *did* decide by the output (`CONTINUE_IF_NOT_QUIET`): they kept processing as long as the output was audible, including an endless self-oscillation, and stopped a silent Gneiss at once. Now iPlug's local tail returns `CONTINUE` for exactly 15 s of silent input, then `SLEEP`: it keeps a silent Gneiss awake for 15 s and cuts a sustained self-oscillation at 15 s.
  - The Log's (3) list states this correctly; the commit message (unpushed, so it can be reworded) and the headline "hosts no longer cut Gneiss's ring" should match it. Suggested wording: "CLAP hosts without the tail extension: was CONTINUE_IF_NOT_QUIET (the host watched the output), now CONTINUE for 15 s of silent input, then SLEEP."
- **M2 (minor) — the mac/win behaviour list for the tail is incomplete (Log (3) and the commit message).**
  - VST2 (the Windows project `Gneiss-vst2.vcxproj` exists): `effGetTailSize` now returns 2 instead of 1. That is iPlug's encoding (`IPlugVST2.cpp:767`, `min(2, tailSize)`): "has a tail" instead of "no tail"; it never reports 15 s.
  - AUv3 (iOS project): `tailTime` is 15 s.
  - Hosts that size offline bounces/freezes, or auto-suspend, by the reported tail may now run up to 15 s longer after the material ends. Logic's "Include Audio Tail" reads AU `TailTime`; Cubase/Nuendo suspend VST3 plug-ins by `getTailSamples`. This is worth one line for the user; the effect depends on the host and I did not verify it per host.
  - AAX: no change; the wrapper has no tail.
- **M3 (minor) — the C5 display-change list misses one case.** For damping release in [9.95, 10] ms, the old display printed "R 10.0 ms" (`v > 10` was the integer branch) and now prints "R 10 ms".
  - This follows from the code: segment `{10, ms, 1}` hands values ≥ 9.95 to the next segment.
  - The 8001-value dump has no sample in that 0.05 ms window, so `display-text-diff-linux.txt` cannot show it.
  - Add it next to "H 10.0 ms → H 10 ms" in the Log and the `3e0cb40` message. The x.5-ms tie change (round-half-even) is in the Log but not in the commit message either.
- **M4 (minor, user decision) — acceptance "every parameter's display text parses back" is not fully met.**
  - Two Gneiss params, `pt tilt` and `preFlt band mode`, still fail the round trip in the sliver (−0.0005, 0): "-0.000" → "0.000", 16 FAIL. By the same mechanism the 8 UB params' slivers are narrower than the grid (T28-a).
  - The cause is iPlug's default display, so it is reported as T28-a. It is a Gneiss parameter all the same, and clap-validator `param-conversions` can hit it by chance (rarely).
  - Options for the user:
    - fix T28-a in iPlug (one place, all plug-ins);
    - or give these bipolar params a `param_text.hpp` display with the same zero snap — a few lines, no state change.
  - Either is fine; it needs a decision.
- **M5 (minor, for the coordinator) — merge order for `scripts/known-clap-failures/Gneiss.txt`.** It is emptied on `linux/T-028`. If `linux/T-028` merges into `hvoya-linux` before `fix/t028-input-text-tail` reaches `linux-experiment`, `package-smoke-in.sh` reports Gneiss's 4 old failures as new. The Log says so; the coordinator should merge the two together, or the Gneiss side first.
- **N1 (nit)** — the Log says "OnReset only stores host-facing state, nothing of the audio thread". IPlugCLAP's `process()` does read `mTailSize`/`mTailUpdate`. With FL Studio's activate-during-process, that is a formal (benign, same-value) race on plain ints in iPlug's design, and ARCHITECTURE §7's "OnReset only raises `_audioResetPending`" is now one call less literal. A wording fix is enough. Calling `declareTail()` only when the rate changed would also drop the redundant `tailChanged()` on every activate; that is optional.
- **N2 (nit)** — the 15 s tail is a lasting product decision, but it is documented only in the `Gneiss.hpp` comment and the commit. One line in ARCHITECTURE.md (§1 or §7) would follow the doc-maintenance rule.
- **N3 (nit)** — `makeDisplay` formats `offValue` into a fresh `WDL_String` on every display call. It is UI/host thread only, so this is not a correctness issue; the off text could be formatted once at build time.
- **N4 (nit)** — ring parser:
  - "TANH > ASINH 30%" (a space after `>`) reads as TANH 0%, because `rest` is not `trimLeft`ed before the second name is skipped;
  - the second name is not checked ("TANH>TASSYM 30%" reads as TANH→ASINH 30%). The comment says it is implied; that is acceptable.
  - Pre-existing in old and new code alike: the sat-type display indexes `names[int (v) % n]` unguarded, so a host passing an out-of-range or NaN normalized value to `value_to_text` (iPlug does not clamp doubles there) reads out of bounds.
- **N5 (nit, evidence)**:
  - `clap-validator-linux-x20.txt` starts with two stray build lines, and "base sleep-test fail lines: 20 2" is cryptic. The raw per-run outputs were not kept.
  - The three status lines in `tail-status-linux.txt` are unlabeled: after@48k, base, after@96k, as far as I can tell from the Log.
  - The VST3 count is from `validator -e`.
  - The `t028` Docker container is still running: remove it at handover (AGENTS §4.5).

### 2026-10-18 (4) — executor's fixes after review round 1

**Commit messages rewritten** (the branch is unpushed; trees identical, checked with `git diff` before moving the ref): the old SHAs in the sections above map as `3e0cb40` → **`36e1712`** (C5), `2f84c42` → **`75109bb`** (tail), `74552f1` → **`f3b6b77`** (docs). `b600156` (C3) is unchanged.
- **M1** — the tail message now says what happens per host kind: with the tail extension, the host may stop calling 15 s after silence instead of at once. Without it, Gneiss used to return `CONTINUE_IF_NOT_QUIET`: a silent Gneiss slept at once and a self-oscillation ran on. Now it returns `CONTINUE` for 15 s of silent input, then `SLEEP`: a silent Gneiss stays awake 15 s, and a self-oscillation is cut at 15 s.
- **M2** — added to the tail message and to the list below: VST2 `effGetTailSize`; AUv3 `tailTime`; hosts that size a bounce or freeze by the tail (e.g. a bounce that includes the tail) may run up to 15 s longer. Not checked per host.
- **M3** — added to the C5 message: "R 10.0 ms" → "R 10 ms" for 9.95–10 ms, and ties at exactly .5 ms (hold/release/glide) now round to even.

**Corrections to this Log:**
- **N1** — the (3) section says OnReset "only stores host-facing state, nothing of the audio thread". That is not quite right: iPlug's CLAP `process()` reads the tail fields that `SetTailSize` writes. This is the same benign race iPlug's design always has (an `int` and a `bool`). Gneiss's own audio-thread state is still untouched.
- **M2, completing the tail list in (3):** VST2 `effGetTailSize` and AUv3 `tailTime` also report the 15 s now.
- **VST3 validator:** every "VST3 validator ×10" line above was run with `-e` (537 tests); a plain run has 47.

**New commits on the Gneiss branch:**
- `6f829ad` — `param_text`:
  - **N3:** the off text is formatted once, when the display is built.
  - **N4:** the sat-type display clamps a value a host passes out of range (or NaN) to [0, 3] before indexing the names; the parser reads "TANH > ASINH 30%" (spaces around `>`).
  - Display only. The 8001-value text dump is byte-identical to before this commit. The new typed case passes.
  - clap-validator 38/0 ×3. Renders 54/54 + states 54/54 still bit-identical to base (Linux, rebuilt).
- `725dfa7` — **N2:** `ARCHITECTURE.md` §7 records the 15 s tail, why it is that long, and that VST3/AU take it only statically.

**N5:** `clap-validator-linux-x20.txt` and `tail-status-linux.txt` were rewritten with labels. The `t028` container is removed.

**M4 (user's decision, not done):** the "-0.000" round trip (T28-a) remains for the bipolar params on iPlug's default display: `pt tilt` and `preFlt band mode`, and by the same mechanism the 8 UB params, which the sampled grid did not hit. Options:
- (a) fix it once in iPlug's `IParam::GetDisplay` (shared, all plug-ins);
- (b) give these 10 Gneiss params a `param_text.hpp` display with the same text as now (`%.3f` / `%.4f`) and zero snapping. A few lines, no state or audio change.

**M5 (coordinator):** merge `known-clap-failures/Gneiss.txt` (now empty) only together with, or after, the Gneiss fix in `linux-experiment`.

Gneiss branch now: `b600156` C3 · `36e1712` C5 · `75109bb` tail · `f3b6b77` docs · `6f829ad` param_text review fixes · `725dfa7` ARCHITECTURE tail.

### Review round 2 (independent reviewer)

**Verdict: approved.** M1–M3 and N1–N5 are resolved. Two nits remain (below); both are wording, and neither blocks the merge. M4 is left to the user and M5 to the coordinator, as stated in (4).

**Checked:**
- **Rewritten commits.** `git diff` shows identical trees: `3e0cb40`/`36e1712`, `2f84c42`/`75109bb`, `74552f1`/`f3b6b77`.
- **M1:** the `75109bb` message now describes each host kind correctly. CLAP without the tail extension: `CONTINUE_IF_NOT_QUIET` → `CONTINUE` for 15 s of silent input, then `SLEEP`. A silent Gneiss stays awake 15 s; a self-oscillation is cut at 15 s.
- **M2:** VST2, AUv3 and bounce/freeze are now listed, marked "not checked per host".
- **M3:** the `36e1712` message now has "R 10.0 ms → R 10 ms" (9.95–10 ms) and the .5-ms ties rounding to even.
- **N1:** the Log is corrected in (4).
- **N5:** `clap-validator-linux-x20.txt` and `tail-status-linux.txt` are labeled and readable now, and the `t028` container is gone.
- **`6f829ad` code:**
  - The off text is formatted once and captured as a `std::string`. It is compared only when `offText` is set, as before.
  - The sat-type display maps NaN → 0 and clamps to [0, n] before `int`/indexing. `n` gives `i = n`, `names[n % n]`, the same as before for in-range values.
  - The parser `trimLeft`s after `>`; the result is still a suffix of the input, so it stays NUL-terminated for `strtod`.
  - Includes `<algorithm>` and `<string>` added. The new comment is a WHY.
- **`725dfa7`:** the ARCHITECTURE §7 tail bullet is accurate and short.
- **Re-run on `build/t028-after`.** Built 13:01 from `tmp/t028-linux` afd1d8f; its `param_text.hpp` equals `725dfa7`'s, and its other differences are `linux-experiment`'s own. Outputs are in `build/t028r-val/`, not committed.
  - param-text: `FAIL=16 JUMP=13 nan-fails=41 typed-fails=0`. The fail/jump lines are identical to round 1, and the new typed case `'TANH > ASINH 30%' → 'TANH>ASINH 30%'` passes.
  - Renders vs base: **54/54 render + 54/54 state identical**. Input changed 0, allocations 0, non-finite 0.
  - clap-validator ×3: 38 pass / 0 fail / 0 warnings.

**Remaining nits (wording, optional):**
- **R2-N1:** `75109bb`'s message and (4) say VST2 `effGetTailSize` reports "15 s instead of 0". iPlug's VST2 wrapper encodes any finite tail as 2 (`IPlugVST2.cpp:767`, `min(2, tailSize)`): 2 ("has a tail") instead of 1 ("no tail"). It never reports 15 s.
- **R2-N2:** ARCHITECTURE §7's next bullet still says `OnReset` "only raises `_audioResetPending`". The tail bullet right above it says the tail is set from every `OnReset`. A short "(besides the host-facing tail)" would remove the contradiction.
