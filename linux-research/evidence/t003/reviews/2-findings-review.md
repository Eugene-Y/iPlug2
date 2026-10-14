# T-003 review 2 — bug findings and their classification (independent subagent, 2026-10-14)

Scope: every bug the engineer reported (L1–L3 Linux-port, C1–C6 cross-platform, X1–X3 Linux-specific consequences), checked for "Linux WIP or really shared?". Report only; the reviewer changed nothing (its scratch files are in the session scratchpad `rev2/`). Verbatim report below.

---

## T-003 findings review

**What I ran:** one mac clang test (`scratchpad/rev2/ab.cpp`). It builds the real `hvoya/hvoya/utils/audio_buffer.cpp` with a stub logger and replays Gneiss' call order. I also ran `objdump -T` on the 22.04 `Gneiss.clap`. **Everything else was read only:** code plus the existing evidence. I did not use Docker or a mac host. Nothing was modified, apart from new files in `scratchpad/rev2/`.

Summary: four of the six "not Linux" findings hold as stated, and two need correcting. The real problems are three classifications:
- L1 is labelled Linux-only, but its root cause is shared.
- X1 is labelled a Linux-only consequence, but it is a shared iPlug race.
- C3 describes the input overwrite as "OS 1x only"; it happens at every OS factor.

### Linux-port bugs

**L1 (editor shows only the top 200 px in X11 hosts) — PARTLY.** The Linux symptom and fix are right; the "Linux-only" label is not established.
- Gneiss creates the editor already at the morph-on height: `MakeGraphics(..., currentUIHeight(), ..., scale)` at `Gneiss.cpp:183`. `OnUIOpen`→`rebuildMorphLayout` then returns early because the height already matches (`GneissUI.cpp:1169`).
- So nothing ever updates the delegate's stored size (`mEditorWidth`/`mEditorHeight`), and the host is never told.
- `mInOpenWindow` is an hvoya-local shared change (c8234d338, July 2026, made for AU). It sits in `IGraphics.cpp:85,115` and is set in `IGraphicsMac.mm:103-110` and on Linux.
- On mac, the CLAP size query (`IPlugCLAP.cpp:972`) and the VST3 one (`IPlugVST3_View.h:76`) return the same stale size as on Linux: 200 px tall. The width is also wrong when the UI scale is not 1, because it reports `PLUG_WIDTH` unscaled.
- "Mac hosts follow the NSView frame" is only guaranteed for AU. For mac VST3/CLAP it depends on the host. I could not test a mac host.
- **Windows claim: CONFIRMED.** `IGraphicsWin` never sets `mInOpenWindow`. Its `OpenWindow` calls `SetScreenScale` (`IGraphicsWin.cpp:1071`), which tells the host and updates the stored size, and `PlatformResize` also resizes the parent window.
- Classification: shared hvoya/iPlug behaviour plus Gneiss sizing its editor from state. The Linux commit is a Linux-only workaround. A shared fix would sync the stored editor size after open in `IGEditorDelegate`/`IGraphics`.
- Severity: high on Linux (fixed); unknown on mac VST3/CLAP.

**L2 (standalone window not resized on Linux) — CONFIRMED, Linux-port.**
- On Windows, `IPlugAPP::EditorResize` returns false, so `IGraphicsWin::PlatformResize` resizes the parent window `gHWND` itself (`IGraphicsWin.cpp:816-836`).
- `IGraphicsLinux::PlatformResize` only resizes its own X window (`IGraphicsLinux.cpp:1143`).

**L3 (invisible stand-in menus) — CONFIRMED, Linux-port.**
- Gneiss only loads RobotoMono and the MDI font (`GneissUI.cpp:155-156`) and never calls `AttachPopupMenuControl`.
- mac and Windows `CreatePlatformPopupMenu` use native menus (NSMenu, HMENU). `DEFAULT_FONT` is "Roboto-Regular" (`IGraphicsConstants.h:59`).

### Findings reported as not Linux

**C1 (`AudioBuffer::reserve` leaves stale channel pointers) — CONFIRMED, I ran it. hvoya utils bug; Gneiss is currently the only caller. Severity: high (heap corruption on mac/win).**
- `reserve` (`audio_buffer.cpp:210-214`) reallocates `_data` but does not rebind `_channels`.
- In Gneiss, `_osBuffer` starts at the default 2×512 and is reserved to 2×4096 in the constructor (`Gneiss.cpp:189`→`1767`).
- `OnReset` only sets a flag. `resetAudioState` and `updateOversampling` do not touch `_osBuffer`. The only resize points are `setNumChannels`/`setNumFrames` in `processChunk` (`:819-820`).
- `_dryBuf` (reserve 2×512 matches its existing capacity) and `_mixWorkBuf` are unaffected; the test confirms both.
- Test output: `2x@256: chan0 inside live storage? NO (stale)`.
- Nuance the engineer missed: the bug needs the first oversampled chunk to be stereo with block × OS factor = 512 (256 @2x, 128 @4x, 64 @8x).
- Any other size, or a mono block, calls `resize()`, which rebinds the pointers for good. After that it is safe.
- The default is 1x, so it is triggered by switching to 2x at a 256 block, or by loading a project saved with that setting. That is a common setup.
- "Affects every product" is overstated: grep across `/Volumes/Storage/code/hvoya` finds only Gneiss calling `reserve`.

**C2 (morph toggle needs extra clicks after a preset load) — CONFIRMED. Gneiss logic, with the hvoya PresetManager restore order contributing. No platform or format code involved.**
- Preset strip path: `PresetManager::applyIdx` (`preset_manager.hpp:~860`) calls `RestorePreset` (`IPlugPluginBase.cpp:425`). That runs `GneissStateCodec::unserializeParams`, then `OnParamReset(kPresetRecall)` (`gneiss_state_codec.hpp:129`).
- That calls `OnParamChange(morph_active, 1)`. `isSettingNewVal` stores 1, and `onMetaParamChanged` briefly sets `_active=true`.
- Then `restoreWorkspace` → `applyMorphModeParams(0)` (`morph_controller.hpp:~1980`) uses a direct `IParam::Set(0)`, sets `_active=false`, and resyncs the UI.
- The dedup cache `_prevParamVals[morph_active]` stays at 1, so the next UI click sending 1 is dropped (`Gneiss.cpp:1940`, `Gneiss.hpp:574`).
- Why recall sets morph to 1 at all: `makeChunkPreset` (`factory_preset.hpp:63-88`) serializes the whole live state at construction, and `par_morph_active` defaults to true (`Gneiss.cpp:1174`). So every factory preset carries morph_active=1.
- It will reproduce in AU/VST3/CLAP on mac. The host is not involved, since the preset strip calls `RestorePreset` internally.
- Extra side effect: `_active` is briefly true between restore and workspace reapply. An audio block running concurrently can morph for that instant.
- Minor: after recall the MORPH button is resynced to 0 (unlit), not "lit". It only lights after the swallowed click.

**C3 (Gneiss overwrites the host's input buffer) — PARTLY. Gneiss logic, not Linux. Severity: low–medium, depends on the host.**
- The mechanism is wrong in the write-up: the input is overwritten at **every** OS factor, not just 1x.
- `_oversampler.downsample(_osBuffer, input)` (`Gneiss.cpp:870`), then the mix/gain multiplies and `input += _dryBuf`, all write to `input`. Only then is it copied to `output`.
- "Float hosts are safe" is correct: `IPlugProcessor::AttachBuffers` for the source sample type copies into iPlug's own scratch buffer (`IPlugProcessor.cpp:470-495`). The double-precision overload attaches host pointers directly (`:455`).
- Gneiss is built double on mac/win. CLAP declares support for, and preference of, 64-bit (`IPlugCLAP.cpp:778`). VST3 accepts `kSample64` (`IPlugVST3_ProcessorBase.cpp:214,246`).
- So mac/win 64-bit hosts that process out of place get their input clobbered. AU v2 is float and therefore copied.

**C4 (VST3 host sees old values after the preset-strip '>') — CONFIRMED. Shared iPlug VST3 behaviour plus Gneiss/hvoya not notifying the host. Not Linux; mac VST3 will behave the same.**
- VST3 `getParamNormalized` returns the controller's cached parameter objects (`IPlugVST3_ControllerBase.h:194`). That cache is only updated through `SetVST3ParamNormalized`, for example via `DirtyParametersFromUI`, which nothing calls after a preset-strip load.
- `IPlugVST3::InformHostOfPresetChange` is empty (`IPlugVST3.h:62`).
- CLAP only looks right because `paramsValue` reads the live `IParam` (`IPlugCLAP.cpp:500`). No parameter events are sent, so CLAP hosts that cache values won't see the change either.

**C5 (clap-validator `param-conversions` and `process-sleep-process-status`) — CONFIRMED that neither is Linux.**
- `param-conversions`: Gneiss' `hzDisplay` shows "1.06 kHz", and iPlug's `StringToValue` just does `atof` (`IPlugParameter.cpp:397`), giving 1.06 Hz.
  - All Hz parameters fail the same way, and so does `gain on dry` ("50%" → 50 → clamped to 1). The validator stops at the first failure.
  - The `paramsValueToText` edits on hvoya-linux are not OS-gated but don't affect these double parameters.
- `process-sleep-process-status`: Gneiss never sets a tail size, so iPlug CLAP returns `CONTINUE_IF_NOT_QUIET` (`IPlugCLAP.cpp:396`). The resonant filter then rings at about -55 dBFS.
  - IPlugEffect passes this test in every earlier evidence set.
  - I couldn't check the validator's exact rule (no source available). Most likely a Gneiss tail declaration, platform-independent.

**C6 (knobs disabled in morph with no point selected, so no CC menu) — CONFIRMED by design.** `controlsCaptured()` is defined at `morph_controller.hpp:159`, and `ControlDecorator::CreateContextMenu` deliberately returns early when the control is disabled (`control_decorator.hpp:434-440`).

### Linux-specific consequences

**X1 (VST3 validator crash in `SetLatency`) — REFUTED as Linux-only. It is a shared iPlug thread-safety race.**
- `IPlugProcessor::SetLatency` → `NChanDelayLine::SetDelayTime` (`NChanDelay.h:26-31`) changes the delay length, then reallocates and clears the buffer, with no synchronization.
- The audio thread reads that buffer in `PassThroughBuffers`, which runs only while the plug-in itself is bypassed: VST3 `GetBypassed` (`IPlugVST3_ProcessorBase.cpp:399`), AU (`IPlugAU.cpp:1740`), AAX.
- In real mac/win hosts, `OnIdle` on the main thread runs concurrently with the audio thread too. So an OS-factor change while bypassed, through the plug-in's own bypass, can crash there (use-after-free or out-of-bounds read).
- REAPER is unaffected because it normally bypasses FX itself and never engages the plug-in's bypass, not because `OnIdle` is driven by the host.
- The genuinely Linux-only part: in validators with no event loop, the fallback timer thread runs `OnIdle`. mac and Windows validators never fire timers, so it shows up there only on Linux.
- Severity: low (narrow window), but it is a crash.

**X2 (GLIBC floor 2.35) — CONFIRMED.**
- `objdump -T` shows only `hypot` and `hypotf` at GLIBC_2.35. The callers are Gneiss `morph_field_overlay.hpp` and `point_arrangement.hpp`, plus `hvoya/utils/user_tab_panel.cpp:738`.
- Missed: the pthread and `dl*` symbols already set a floor of 2.34, so `.symver`-pinning hypot only lowers it to 2.34, still well above 20.04's 2.31.

**X3 (macOS IPlugEffect APP target doesn't build) — CONFIRMED pre-existing, plausibility only, not built.**
- `#import <Cocoa/Cocoa.h>` under `OS_MAC` comes from hvoya commit cf2b2a1f4 (June 2026), before any Linux work, so the main `hvoya` branch is affected too, not just hvoya-linux.
- IPlugEffect's xcodeproj compiles `IPlugAPP.cpp` as plain C++ (`lastKnownFileType = sourcecode.cpp.cpp`, pbxproj:521).
- Gneiss' project forces Objective-C++ (`explicitFileType = sourcecode.cpp.objcpp`), which is why Gneiss builds and the example doesn't.

### New issues
1. **Stale editor size on mac (from L1):** VST3/CLAP report a stale size whenever the editor is created at a size other than the stored one (morph on, or UI scale ≠ 1). Worth checking in a mac host such as Cubase or Live, which size the window from getSize.
2. **Input overwritten at all OS factors (C3):** the T-010 fix must cover the downsample target and the gain/mix stage, not only the `work = input` line.
3. **Missing host notification after preset loads (C4):** the fix is for Gneiss/hvoya `PresetManager` to call `DirtyParametersFromUI()` after a load. Optionally, iPlug VST3 could implement `InformHostOfPresetChange`.
4. **Dedup cache vs direct `Set` in Gneiss (C2):** every direct `IParam::Set` followed by a UI resync can desync `_prevParamVals`. `applyMorphModeParams`, `snapshotPatchMorphOff` and `morph_file_router` all do this. Fix by updating `_prevParamVals` in `applyMorphModeParams`, or exempting meta parameters from the dedup.
5. **Shared `SetLatency` race (X1):** should be fixed in iPlug, or Gneiss should only change latency while not processing. Deferring it alone does not remove the race.
