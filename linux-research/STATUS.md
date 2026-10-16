# Coordinator status — snapshot 2026-10-16

This is the coordinator's handover file: where things stand and what waits on whom. A cold session reads it after `AGENTS.md`.
- Task-level detail lives in `tasks/README.md` and the task files.
- Design and decisions live in `PLAN.md` §1–2 and `notes/`.

Update it at every merge or user decision.

## Branches and repos

| Repo / branch | Where | State |
|---|---|---|
| iPlug2 `hvoya` (product) | `/Volumes/Storage/code/IPlug2` (main checkout, Gneiss builds against it) | Pushed to `gene` up to 3474f3d55. My commits there, all with the user's OK: logger snprintf fix 46f834a92, `IText::SetFont` 3a05dd2f4, **upstream master merged** 5d854712d, `ITextEntryControl` right-click menu UAF (H1) e0c2680e7, **C1 `AudioBuffer::reserve` fix 3474f3d55**, CLAUDE.md pointers. **The user has not yet tested Gneiss on mac/win after the upstream merge.** |
| iPlug2 `hvoya-linux` (Linux integration) | `/Volumes/Storage/code/IPlug2-linux` | Pushed to `gene` up to b33e405b2 by the user on 2026-10-16; later commits are unpushed until the user pushes. Contains hvoya at 5d854712d + Linux P0–P5 + T-001/2/3/7 merges + the C1 cherry-pick e7c882995. HEAD ≈ 30580fe02 |
| Gneiss `master` | `/Volumes/Storage/code/hvoya/Gneiss` (user's working tree, uncommitted .RPP/xcscheme edits) | 3cf2e2e (user's morph/preset-swap fix, 2026-10-16), pushed |
| Gneiss `linux-experiment` | `/Volumes/Storage/code/hvoya/Gneiss-linux` (local only, never push) | master 3cf2e2e merged (4219350) + 52cb5d6 (CMakeLists.txt) + 7e3a1c9 (initializer_list UB fix, behaviour-neutral) |
| Sand | `/Volumes/Storage/code/hvoya/Sand` | Untouched. T-012 must use a local experiment branch + worktree, like Gneiss |

## In flight
- **T-005 merged** (2026-10-16). T-011 is now ready.
- **T-008 merged** (fac9cc84d). **Open VM-only re-check of its final focus build** (light, short): `scripts/vm/vm-run.sh vm/probe/focus-edit.sh` CLAP + VST3 ×10 and the APP P3b script ×3. Earlier VM passes used the first focus version. Check `timeout 8 xdpyinfo` before and after.

## Waiting on the user
1. ~~Gneiss morph bugs on mac~~ — **fixed by the user 2026-10-16** (hvoya 0b2d8dbef, Gneiss 3cf2e2e) and synced into hvoya-linux (e393b6398) / linux-experiment (4219350); Linux smoke PASS. Old note: Then the coordinator merges Gneiss `master` → `linux-experiment` and `hvoya` → `hvoya-linux`, with builds plus REAPER runs (`steps/gneiss.txt`).
   - **Hints given to the user:**
     - C2 analysis in `CROSS_PLATFORM_FINDINGS.md` and `evidence/t003/reviews/2-findings-review.md`: the `_prevParamVals` dedup cache is not updated by direct `IParam::Set` in `applyMorphModeParams`/`snapshotPatchMorphOff`/`morph_file_router`.
     - To check whether the upstream merge is to blame: build Gneiss against hvoya `3a05dd2f4` (pre-merge) vs `5d854712d` (post-merge).
2. **Cross-platform findings** in `CROSS_PLATFORM_FINDINGS.md`. C1 is fixed. Open for the user's decision: X1 (`SetLatency` race), C3 (input buffer overwrite in 64-bit), C2/C4 (preset/morph/host notification), L1 (stale editor size on mac VST3/CLAP), C5 (text→value parsing, tail).
3. **Push `hvoya-linux`.** The auto-mode classifier blocks pushes from Claude.
   - The `gene` remote is https without stored credentials. Push via `git@github.com:Eugene-Y/iPlug2.git`, or the user runs `git remote set-url gene git@github.com:Eugene-Y/iPlug2.git`.

## Decisions already made by the user (don't re-ask)
- **T-005 m2 (2026-10-16): keep as is.** The examples' Linux-only `CMakePresets.json` stays; the empty presets list in VS Code CMake Tools on mac/win is documented in `Documentation/linux-development.md` (workaround `cmake.useCMakePresets: never`).
- **IDE (2026-10-16, provisional):** VS Code is the primary Linux IDE; IPlugEffect and IPlugControls are the templates to develop in and duplicate from. Qt Creator is the documented alternative; CLion only with a paid licence (its free licence forbids sold products). notes/20, `Documentation/linux-development.md`.
- **Rough edges must be fixed, not just noted (2026-10-16):** every known gap gets a task file. For T-011 this includes changing the shared `Examples/duplicate.py` (out-of-tree path rewrite), which also changes mac/win output (for the better).
- The port branches from `hvoya`, never upstream. Upstream master **was merged into hvoya** (2026-10-11).
- RR 360d84011 (StringToValue numeric fallback for enum/bool) is **rejected**.
- v1 = CLAP + VST3 + APP, NanoVG GL2, X11. **LV2 is the last phase (P12)**, after P11 (developer experience: IDE research — CLion is non-commercial-only; presets, `duplicate.py`, one-command flows, for IPlugEffect/IPlugControls/Gneiss/Sand).
- Gneiss/Sand experiments only on local branches in separate worktrees. Never touch the main checkouts.
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

## Next tasks (after T-008), in suggested order
- T-013: host matrix on the VM — Carla, Ardour VST3, Renoise. Bitwig and x86_64 need a PC.
- T-004: standalone robustness.
- T-006: `state-reproducibility`.
- ~~T-005: IDE research~~ → merged; next **T-011** (duplication out of tree + Gneiss IDE setup).
- After the Gneiss master sync: T-009 (visual parity) and T-010 (Gneiss-side Linux needs: deferred `SetLatency`, menus, `openFolder`; keep cross-platform fixes out unless the user decides).
- T-012: Sand.
- T-011: templates.
- T-014: LV2.
