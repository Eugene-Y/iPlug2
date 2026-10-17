# Coordinator status — snapshot 2026-10-17 (end of the first coordinator's session)

This is the coordinator's handover file: where things stand and what waits on whom. A cold session reads it after `AGENTS.md`; a new **coordinator** then reads `COORDINATOR.md` (how the role works).
- Task-level detail lives in `tasks/README.md` and the task files.
- Design and decisions live in `PLAN.md` §1–2 and `notes/`.

Update it at every merge or user decision.

## Branches and repos

| Repo / branch | Where | State |
|---|---|---|
| iPlug2 `hvoya` (product) | `/Volumes/Storage/code/IPlug2` (main checkout, Gneiss builds against it) | **Local commits since 3474f3d55/0b2d8dbef, unpushed:** T-024 merges a865ee9f7 (F3 Hilbert), 228f2efc2 (F4 TextToValueFunc), 083bc87c5 (F2 MIDI CC → host on the main thread: every plug-in with a mediator needs `_midiCCMediator.OnIdle()`), fa98170e8 (cherry-pick ef002e167, CLAP stepped-param text). Earlier, all with the user's OK: logger fix, `SetFont`, upstream merge 5d854712d, H1, C1, CLAUDE.md. T-006's CLAP rescan cherry-picked as f71ddd876 (user, 2026-10-17). T-021's fixes committed there by the 2nd instance at the user's decision: 59c33feec (hvoya `HostInfoModel` SIGFPE on x86, T21-2), 228f1ea8e (VST3 tempo/time signature only when valid, T21-3). **HEAD 228f1ea8e, unpushed.** mac builds of Sand + Gneiss CLAP/VST3/APP against it (at 083bc87c5+) PASS (2026-10-17) |
| iPlug2 `hvoya-linux` (Linux integration) | `/Volumes/Storage/code/IPlug2-linux` | Pushed by the user up to b33e405b2 (2026-10-16); **everything after is unpushed** (HEAD ab632ec4e + this handover). Contains hvoya up to 228f1ea8e, Linux P0–P5, and the merges of T-001…T-008, T-012, T-021, T-024 (merge log in `tasks/README.md`) |
| Gneiss `master` | `/Volumes/Storage/code/hvoya/Gneiss` (user's working tree, uncommitted .RPP/xcscheme/main.rc_mac_dlg edits) | 4dc02eb (`_midiCCMediator.OnIdle()`, needs hvoya 083bc87c5; committed at the user's command 2026-10-17, unpushed) on 3cf2e2e |
| Gneiss `linux-experiment` | `/Volumes/Storage/code/hvoya/Gneiss-linux` (local only, never push) | 52cb5d6 (CMakeLists.txt), 7e3a1c9 (initializer_list UB fix, behaviour-neutral), 3b9767e (`scripts/makedist-linux.sh`, T-021; the user kept it, R12), master merged up to 4dc02eb (8a0f05b, 2026-10-17) |
| Sand `master` | `/Volumes/Storage/code/hvoya/Sand` (user's main checkout) | 54be993 merge of T-024 `fix/cross-platform-f1-f8` (at the user's command 2026-10-17, unpushed) on the user's 961be6c |
| Sand `linux-experiment` | `/Volumes/Storage/code/hvoya/Sand-linux` (local only, never push) | T-012: 4dcd5cd CMake, 2456460 `std::isfinite`, 60c5b96 presets + workspace, e259e96 C++23, cec9c1d `.clangd` → `compile_flags.txt` (user's D1). Needs GCC 13 on 22.04 (`iplug2-linux:22.04-gcc13`); master 54be993 merged (a027e6d, 2026-10-17) |

## In flight
- **T-026** (Sand follow-ups: N1 audio-thread allocation, N2–N4, F2 host checks) — **claimed by the Sand chat** 2026-10-17 21:54 (lock `T-026`; worktrees `IPlug2-T-026-log` on `linux/T-026`, `hvoya/Sand-t026` on `fix/t026-followups`, `Sand-t026-base`, `Sand-t026-linux` on `tmp/t026-linux`). Merge it like T-024 when the user says so: Sand fixes into Sand `master` (main checkout) only at the user's command; any shared hvoya change is a proposal for the user first.
- **Open VM-only re-check of T-008's final focus build** (light, short; coordinator): `scripts/vm/vm-run.sh vm/probe/focus-edit.sh` CLAP + VST3 ×10 and the APP P3b script ×3, `timeout 8 xdpyinfo` before and after. Needs the VM up (ask the user).
- Nothing else is mid-merge. Latest release archives (T-021 flow on the merged tree, smoke PASS): `linux-research/build/m021-dist/` (ignored, local only).

## Waiting on the user
1. **Pushes** (Claude never pushes; the classifier blocks it): iPlug2 `hvoya` (HEAD 228f1ea8e), `hvoya-linux` (everything after b33e405b2), Sand `master` (54be993), Gneiss `master` (4dc02eb). Product `linux-experiment` branches stay local. The `gene` remote is https without stored credentials: push via `git@github.com:Eugene-Y/iPlug2.git`, or `git remote set-url gene git@github.com:Eugene-Y/iPlug2.git`.
2. **After T-024 (F2):** add `_midiCCMediator.OnIdle();` to `OnIdle` in DMT, Melter, Crust_new (uncommitted one-line changes are ready in `hvoya/DMT-t024`, `Melter-t024`, `Crust_new-t024`; DMT and Melter don't build with current hvoya anyway; Crust_new has the user's own uncommitted edits) and in Scape (not git). Without it their knobs stop following CC (audio still follows).
3. **Windows**: build Sand after F7 (`PolyphaseIIR2Designer.cpp` added to the vcxproj); T-026 will touch `Sand-aax.vcxproj` too.
4. **F2 in other hosts** (AU, AAX, VST2) and the possible ≤ 20 ms value step-back at the end of a fast CC sweep in VST3 — T-026 prepares checks/checklists; FL Studio crash (N4) checklist too.
5. **Cross-platform findings still open** (`CROSS_PLATFORM_FINDINGS.md`): C2/C4 (preset/morph/host notification; T6-1 covers only iPlug's own state load), C3 (Gneiss writes the host's input buffer, 64-bit), C5 (text→value, Gneiss), L1 (stale editor size on mac VST3/CLAP), T8-1..3 (IPopupMenuControl), T21-5 (beat display right only at 120 BPM), T5-1 (fixed in T-011 when it runs). X1 → **T-025** (the user: must be fixed).
6. Gneiss morph bugs (2026-10-16): fixed by the user (hvoya 0b2d8dbef, Gneiss 3cf2e2e), synced.

## Decisions already made by the user (don't re-ask)
- **2026-10-17:** T-004's shared APP fixes (013328ba4 A2/A3, 7cce657a1 A1/A4, bb004604a m7) approved. T-006's CLAP rescan after state load (T6-1) approved, also into `hvoya` (f71ddd876). T-024: all Sand fixes into Sand `master`, the hvoya parts (F2, F3, F4) into `hvoya`, the `OnIdle` line committed in Gneiss `master`; ef002e167 (CLAP stepped-param text) cherry-picked into `hvoya`. T-021: T21-2 and T21-3 fixed on `hvoya`; Gneiss 3b9767e kept (R12); X1 must be fixed (T-025). Sand may be built (its CLAUDE.md "do not build" is outdated). Sand's cross-platform fixes go to a separate chat (T-024, now T-026), not to the Linux instances. After each merged task: clean up its worktrees/branches/containers.
- **2026-10-16:** the repo `README.md` gets the Linux section **last** (P13 / T-015). Only NanoVG GL2 is tested on Linux (noted in code and docs); GL3 is T-020.
- **T-005 m2 (2026-10-16): keep as is.** The examples' Linux-only `CMakePresets.json` stays; the empty presets list in VS Code CMake Tools on mac/win is documented in `Documentation/linux-development.md` (workaround `cmake.useCMakePresets: never`).
- **IDE (2026-10-16, provisional):** VS Code is the primary Linux IDE; IPlugEffect and IPlugControls are the templates to develop in and duplicate from. Qt Creator is the documented alternative; CLion only with a paid licence (its free licence forbids sold products). notes/20, `Documentation/linux-development.md`.
- **Rough edges must be fixed, not just noted (2026-10-16):** every known gap gets a task file. For T-011 this includes changing the shared `Examples/duplicate.py` (out-of-tree path rewrite), which also changes mac/win output (for the better).
- The port branches from `hvoya`, never upstream. Upstream master **was merged into hvoya** (2026-10-11).
- RR 360d84011 (StringToValue numeric fallback for enum/bool) is **rejected**.
- **WebView (2026-10-16): out-of-process WebKitGTK helper embedded via XEmbed** (notes/22 option 1).
- **Priority (2026-10-16): Gneiss and Sand as fully working Linux plug-ins first**; order in `tasks/README.md` "Priority".
- **Feature parity with mac/win is required (2026-10-16):** Linux must not be poorer than mac or win, WebView included. Gaps and tasks: notes/22, T-016..T-021.
- First milestone = CLAP + VST3 + APP, NanoVG GL2, X11. **LV2 is the last phase (P12)**, after P11 (developer experience: IDE research — CLion is non-commercial-only; presets, `duplicate.py`, one-command flows, for IPlugEffect/IPlugControls/Gneiss/Sand).
- Gneiss/Sand experiments only on local branches in separate worktrees. Main checkouts only on the user's explicit command (see `COORDINATOR.md`).
- **Minimal footprint** (AGENTS §2): the port must not change mac/win behaviour, projects or flags. Report cross-platform bugs; don't fix them in port branches.
- Multi-instance workflow (AGENTS §4–5).
  - The coordinator = the main chat: it merges, keeps the board, runs smoke tests.
  - **One subagent at a time.**
  - A **mandatory independent fresh-context reviewer** before hand-over.

## Environment gotchas learned the hard way
- Bash tool runs zsh: unquoted `$VAR` is **not** word-split, so use `bash -c '…'` for loops and `set --`. There is no `timeout` on macOS; use `perl -e 'alarm N; exec @ARGV' cmd`.
- **The macOS clang ASan runtime hangs before `main`** on this macOS (InitializeShadowMemory). Use ASan in the Linux Docker image instead.
- A fresh iPlug2 worktree needs `scripts/install-sdks.sh <worktree>` before any build. It modifies `Dependencies/IPlug/VST3_SDK/README.md`; never commit that.
- REAPER's VST3 host parameter context menu does not open on a same-millisecond synthetic click (`xdotool click`), only on a held right click. Gneiss VST3's right-click harness step therefore fails; this is not a regression.
- After limits or sleep, orphaned agent scripts can keep running for a day: the `verify.sh` loop, test binaries. Check `ps` and kill them.
- **The VM's virgl GPU can hang.**
  - Seen during T-007 mode changes and on 2026-10-16 during T-008.
  - Symptoms: X stops answering (`xdpyinfo` times out), a kworker sits in D state, and `dmesg` shows `drm_atomic_helper_wait_for_fences … blocked`.
  - A guest reboot then hangs at shutdown, so the user must **Force Stop + Start in UTM**. If UTM itself is stuck: Force Quit UTM (and any leftover `QEMULauncher`/`qemu-system-aarch64`).
  - **Don't switch the VM to `virtio-gpu-pci` (no GL).** Tried 2026-10-16: GNOME then shows only a cursor on black (DRI3 error), so keep `virtio-gpu-gl-pci`.
  - **Avoid needless display-mode or scale changes on the VM.** They triggered the hang in T-007.
  - **It hangs under our GL test load.** It happened 3 times by 2026-10-16; the third time, 6 minutes after a fresh boot, during T-008's VM batch.
    - Keep VM runs short and light: a few opens and interactions.
    - Do churn and other GL-heavy loops only in Docker (llvmpipe).
    - Check `timeout 8 xdpyinfo` before and after each VM batch.
    - Open question, worth a task later: does our GL usage (context per editor, swap interval 0, open/close churn) trigger a virgl/ANGLE fence hang? Real drivers may not care, but it is worth a probe with a GL-only loop.
  - **Before removing a worktree, check `git status --porcelain --ignored` too, not just tracked/untracked.** The repo ignores `build-*` and `_*`, so a helper named `build-*.sh` is never committed and dies with the worktree (lost T-024's `build-sand.sh` this way, 2026-10-17).
- Agents must kill everything they start on the VM (APP, REAPER, xclip, jackd) when they finish.
- The VM (UTM, 192.168.64.2) is unreachable when the Mac slept or UTM is stopped; there is no `bridge100`. Ask the user. Key: `~/.ssh/claude_linux_vm`. Never use the password.
- Docker: colima. If it's down: `colima start` with the mounts in AGENTS §3.
- REAPER `linux_audio_mode`: 0 JACK, 2 Dummy Audio (the harness's), 3 PulseAudio; 1 presumably ALSA. Without a working device REAPER never calls `ProcessBlock`.
- **VM state after T-005:**
  - `~/.config/REAPER` is a full profile on PulseAudio (mode 3), no longer the minimal seeded one; the harness uses `/tmp/rhome` and is unaffected;
  - VS Code (+ CMake Tools, clangd, cpptools), clangd 18, Qt Creator 13 and the native build deps are installed;
  - `~/dev/iPlug2` holds a hvoya-linux clone with T-005's files;
  - `~/.clap` and `~/.vst3` hold Debug builds of IPlugEffect and IPlugControls.
- **Starting an IDE on the VM from `vm-display-env`** puts the sudo gdb wrapper on PATH. The debuggee (REAPER) then runs as root. Start IDEs with a plain PATH (`scripts/vm/README.md`).

- **mac tooling** (2026-10-17): Homebrew `rust` is installed (T-006); no clap-validator binary is kept on the Mac (build 0.4.1 in a scratchpad, T-006 Log). `auval` is there. mac hosts installed: REAPER, Bitwig Studio, Ableton Live 11 Lite. Command-line `xcodebuild` of Sand works since F8; build products from a detached temporary worktree into a scratch dir (`scripts/coord/mac-xcodebuild.sh`) so the main checkouts stay clean (builds regenerate `main.rc_mac_dlg` and `plug_build_info.hpp`).
- **Docker images** (2026-10-17): `iplug2-linux:{24.04,22.04,22.04-gcc13}` (+ `-amd64` variants, Rosetta), runtime-only `iplug2-rt:{ubuntu22.04,ubuntu24.04,debian12,fedora}` (+ `-amd64`). Sand needs GCC 13 on 22.04 (`std::format`).
- `DOCK_ENV` splits on spaces and `build-cmake.sh` word-splits `EXTRA_CMAKE_ARGS`: pass multi-word flags via `CFLAGS`/`CXXFLAGS` in a `bash -c` (see `scripts/sand-build.sh`).
- Smoke scripts of earlier merges: `scripts/coord/` (copy and adapt).

## Next tasks
See `tasks/README.md`, section "Priority" (who does what, in which order).
