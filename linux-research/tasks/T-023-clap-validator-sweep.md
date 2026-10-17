# T-023 — CLAP: every clap-validator failure of Gneiss and Sand, Linux and mac

- **Status:** ready
- **Owner:** —
- **Branch/worktree:** `linux/T-023` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-023`. Gneiss/Sand side: local experiment branches in their own worktrees (AGENTS §4), never the main checkouts.
- **Depends on:** T-006 merged (its state fix removes 3 failures from every list below).
- **Conflicts:** `IPlug/CLAP/IPlugCLAP.*` (shared with mac/win); Gneiss and Sand sources.
- **Scope narrowed (user, 2026-10-17):** Sand's cross-platform items (F1, F3, F4, F5 = `process-varying-sample-rates`, `param-conversions`, `process-audio-double-*` for Sand) are fixed in **T-024**. This task covers Gneiss, the Linux-only items (Gneiss `process-sleep-process-status`), the iPlug CLAP items below, and a final re-run of the validators on Sand after T-024 is merged. Run it after T-024, and not in parallel with T-010 (both may touch `IPlug/CLAP` and Gneiss `linux-experiment`).

## Why
The user (2026-10-17): CLAP has needed fixes "here and there" over time. Instead of meeting them one by one in hosts, sweep everything clap-validator finds in the two products at once, on both platforms, and fix or decide each item.

## Known failures (clap-validator 0.4.1, after T-006)
| Test | Who | Platform | What is known | Where it belongs |
|---|---|---|---|---|
| `process-audio-double-{in,out}-of-place` | Gneiss, Sand | Linux + mac | "overwritten an input buffer": the plug-in writes into the host's input buffer; in 64-bit mode iPlug passes host pointers straight through | C3 in `CROSS_PLATFORM_FINDINGS.md`; plug-in side, or an iPlug copy for 64-bit |
| `param-conversions` | Gneiss, Sand | Linux + mac | text→value round trip, `StringToValue` = `atof` ("1.06 kHz" → 1.06) | C5; RR's 360d84011 is **rejected**, don't apply it |
| `process-sleep-process-status` | Gneiss | **Linux only** (mac installed build passes 3/3; Linux fails 3/3 in Debug and Release) | "sleeping but output non-constant"; iPlug never sets `constant_mask`, so the plug-in can only "sleep" via `CLAP_PROCESS_SLEEP` (finite-tail branch of `IPlugCLAP::process`) | T-006 Log; find out first whether it is a port bug |
| `process-varying-sample-rates` | Sand | Linux (mac not run yet) | not analysed | `evidence/t012/validators/clap-validator-Sand.clap.txt` |
| `param-fuzz-bounds` | IPlugInstrument (example only) | Linux | subnormal output | notes/03; low priority |
| `state-invalid-random` (warning) | IPlugControls, IPlugInstrument | Linux | 1 MB of random bytes loads "successfully" | check Gneiss/Sand too |

Also, not from the validator:
- CLAP has no `InformHostOfPresetChange` override: presets loaded by iPlug itself leave CLAP hosts with stale values. Gneiss's PresetManager and morph undo bypass it anyway (C4, T-006 Log).
- `IPlugCLAP::FlushParamsIfNeeded` calls `paramsRequestFlush()` without `canUseParams()`. That asserts in Debug on a host without `clap.params`.

## Steps
1. Build Gneiss and Sand CLAP on Linux (Docker, Debug **and** Release; `DOCK_ENV="BUILD_TYPE=Debug"`, see AGENTS §3) and on mac. For the mac, build xcodebuild from the experiment worktree with `IPLUG2_ROOT=<your iPlug2 worktree>`, `-derivedDataPath` and `CLAP_PATH` in your scratchpad; otherwise its `../../IPlug2` resolves to the main checkout. The user's installed builds under `~/Library/Audio/Plug-Ins/CLAP/IPlug2Dev/` serve as a read-only "before".
2. Run the full `clap-validator validate` on each, Linux and mac, and keep the full logs in `evidence/t023/`. Validator on mac: Homebrew `rust` is installed; build 0.4.1 from `https://github.com/free-audio/clap-validator` (tag `0.4.1`) with `cargo build --release --locked` in your scratchpad (T-006 Log).
3. For each failure: root cause, Linux-only or cross-platform, and which repo it belongs to (iPlug2 shared code, the Linux port, Gneiss, Sand).
4. Fix:
   - Linux-only port bugs: fix in `linux/T-023`.
   - Shared iPlug2 code and the products' business logic: AGENTS §2. Commit a proposal, describe the mac/win behaviour change, add a row to `CROSS_PLATFORM_FINDINGS.md`, and wait for the user's decision before merging.
5. Re-run the validators. Also do a short REAPER check on Linux (`scripts/steps/state-roundtrip.txt`, the products' steps files).

## Acceptance
- [ ] Full validator logs for Gneiss and Sand, Linux and mac, before and after.
- [ ] Every failure and warning has a root cause, a platform answer and an owner (fixed / user decision / won't fix with reason).
- [ ] Linux-only fixes committed; cross-platform ones written up as proposals for the user.
- [ ] Independent review (AGENTS §4); Log complete.

## Log
