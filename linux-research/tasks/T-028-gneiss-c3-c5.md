# T-028 — Gneiss: don't write the host's input (C3), texts parse back (C5), tail / sleep status

- **Status:** ready (user, 2026-10-18: "надо фиксить"); to be run in a separate chat
- **Owner:** —
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
