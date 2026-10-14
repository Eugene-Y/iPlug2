# T-003 review 1 — code (independent subagent, 2026-10-14)

Scope: iPlug2 `linux/T-003` (`git diff hvoya-linux...HEAD`) and Gneiss `linux-experiment` (`d8e95fd..HEAD`), against AGENTS §2, PLAN §1–2 and the user's style rules. Report only; the reviewer changed nothing. Verbatim report below.

---

## T-003 independent review (report only; I edited, committed and checked out nothing)

**Verdict: accept with fixes.** There are no blockers. Two small should-fix items are both in `hvoya/hvoya.cmake`. Everything else is a nit.

### Findings, most severe first

**1. should-fix: the build-info commit SHA goes stale when branch refs are packed.** `hvoya/hvoya.cmake:17-30`, and the comment at `:10-11`.
- Configure watches only `HEAD` and the loose file `refs/heads/<branch>`, and only if that file exists at configure time.
- After `git gc` or `git pack-refs` (auto-gc does this), or on a reftable repo, the branch lives only in `packed-refs`. The ref dependency is then never registered.
- On the next `git commit`, the `HEAD` file doesn't change ("ref: refs/heads/x"). Git writes a new loose ref that nobody watches, so configure doesn't re-run and `PLUG_GIT_COMMIT_SHA` stays old until a manual reconfigure.
- The comment's claim "the commit is never stale" is therefore false.
- `--path-format` needs git ≥ 2.31. On older git (20.04 ships 2.25) the call fails silently under `ERROR_QUIET`, and the dependency is dropped with no warning.
- **Fix:** depend on the reflog `git rev-parse --git-path logs/HEAD`. It is per-worktree and appended on every commit, checkout, reset and amend. Alternatively, generate the header at build time with a `cmake -P` script plus `copy_if_different`, which also makes date and time the build's. Drop "never stale" from the comment.

**2. should-fix: the shared helper now sets a product-level define.** `hvoya/hvoya.cmake:61` adds `$<$<CONFIG:Release>:PLOG_DISABLE_LOGGING>`.
- Gneiss sets this per product, in `config/Gneiss-win.props` `EXTRA_RELEASE_DEFS` and `Gneiss-mac.xcconfig`. It is not in common-win/common-mac.
- IPlugEffect's Xcode and VS configs don't set it. So IPlugEffect's CMake Release now differs from its own Xcode/VS Release, and every future user of the helper (Sand) inherits it silently.
- The comment on `hvoya_add_utils` doesn't mention the define.
- **Fix:** move it to Gneiss' `CMakeLists.txt`, mirroring the props/xcconfig, or make it an explicit argument.
- Side note: `CONFIG:Release` leaves logging on in RelWithDebInfo and MinSizeRel. That matches mac/win (Debug/Release/Tracer), but T-003's own `t003-gneiss-rwdi` build logs.

**3. nit: first-frame size notification.** `IGraphicsLinux.cpp:523-547`. The logic is sound:
- The flag is cleared before the call.
- `open` is a local `shared_ptr` copy, so a host that closes the editor synchronously inside `request_resize`/`resizeView` is handled. Nothing touches `this` after the call.
- `w = Impl()` in `DestroyWindowResources` resets the flag.
- On reopen the sizes match, so there is no close/reopen loop.

Edge cases worth knowing:
- **Host refuses the resize:** `IPlugCLAP`/`IPlugVST3::EditorResize` ignore the result and still call `SetEditorSize`, and there is one attempt only. The host frame stays at 200 while the plug-in believes 620. This is the same semantics as every later UI resize, so not a regression.
- **Host calls `set_size` before the first tick:** `OnParentWindowResize` → `Resize(...,false)` never calls `SetEditorSize`, so `GetEditorWidth()` is stale. The first tick then echoes a redundant `request_resize` of the host's own size. Harmless.
- **Comments:** the flag's comment (`:163`) and the function comment (`:537-539`) say the same thing. Keep one.

**4. nit: stand-in menu font.** `IGraphicsLinux.cpp:1254-1265`.
- The flag is reset with the NanoVG context, which is correct. `nvgCreateFontFaceMem` needs no current GL context.
- `:426` `w.mMenuFontLoaded = false;` is redundant, because `w = Impl()` at the end of the function resets it anyway.
- **Fallback when fontconfig or a TrueType sans is missing:** the menu returns to `DEFAULT_TEXT` (Roboto-Regular). A plug-in that didn't load Roboto then gets the old blank text plus a debug assert, and every menu open re-runs the failed lookup. It's acceptable, but worth a one-line note or a negative cache.
- **Behaviour change:** IPlugControls and every other Linux plug-in now draws stand-in menus in DejaVu/Liberation, not Roboto. This is intended, as parity with native menus on mac/win.

**5. nit: APP resize.** `IPlug/APP/IPlugAPP.cpp:87-98`.
- It moves the window to `rcWindow.left/top`, which SWELL reports as the GDK client position. Under some WM gravity settings that can walk the window by the decoration size on each resize.
- I ran it under openbox and saw no drift. Mutter is untested.
- **Fix:** `SWP_NOMOVE | SWP_NOZORDER` removes the risk, since the position never needs to change.
- The block nearly duplicates the `NO_IGRAPHICS` branch below it.
- WM_SIZE interplay is fine: SWELL sends WM_SIZE synchronously with the new client size. Gneiss has `PLUG_HOST_RESIZE 0`. For host-resizable plug-ins, `OnParentWindowResize` gets the same size, so `Resize` does nothing. That matches Windows, where `IGraphicsWin::PlatformResize` triggers the same path.
- The menu-bar offset holds, because `frameHeight` includes it.
- The hook is a thin `#elif OS_LINUX`, so mac/win are preprocessed out.

**6. nit: Gneiss `CMakeLists.txt:2`.** `project(Gneiss VERSION 1.3.1)` duplicates `PLUG_VERSION_STR` in `config.h`, and iPlug's CMake doesn't use it, so the two can drift. Drop `VERSION`. The default `IPLUG2_DIR` (`../../IPlug2`) resolves to the main `hvoya` checkout, as Xcode/VS do. Without `-DIPLUG2_DIR` it fails at `include(hvoya.cmake)`, which is harmless but worth a line in the comment.

**7. nit: Gneiss source changes.** Both are minimal and correct.
- **`Gneiss.cpp` initializer_list fix:** a conditional over two named const lvalues binds to `InitEnum(..., const std::initializer_list<const char*>&, ...)`, and the named lists' arrays live for the whole scope. The old code was genuine UB on every platform. "Crashed at instantiation" could be misread as template instantiation; "crashed when the plug-in was created" is clearer.
- **`GneissUI.cpp:127-129`:** the hoist is fine. The constants could go in an anonymous namespace to avoid generic file-scope names (`headerH`). Nothing beyond what the build needs was changed.

**8. nit: `dock.sh:31`.** `for m in ${DOCK_MOUNTS:-}` is unquoted, so it word-splits and globs. Paths with spaces are unsupported; say so in the usage comment.

**9. Housekeeping: uncommitted change in the worktree.** `Dependencies/IPlug/VST3_SDK/README.md` is modified and uncommitted: the VST3 SDK fetch overwrote it, about +292 lines. It must not be committed or merged, since Dependencies are not to be modified.

**10. nit: build time and the utils glob.**
- `PLUG_BUILD_TIME` is UTC inside Docker, while mac/win use local time.
- `GLOB_RECURSE hvoya/utils/*.cpp` pulls `user_tab_panel.cpp`, which needs IGraphics, and `hvoya_file.cpp` into IPlugEffect. That matches `common-win.props`, but a future `NO_IGRAPHICS` hvoya plug-in would fail to compile.

### Checked and found correct
- **Sources:** `Gneiss.cpp`, `GneissUI.cpp`, `presets.cpp` and `ui/plugin_control_registry.cpp`, plus the 10 hvoya utils, match `Gneiss-*.vcxproj` and the macOS xcodeproj (which includes `hvoya_file.cpp` and `user_tab_panel.cpp`).
- **Resources:** they match `main.rc` (RobotoMono, MDI font, `gneiss.svg`). Aegean, NotoSansLycian and the hvoya logos are unreferenced in code.
- **`SAMPLE_TYPE_FLOAT`:** it appears only in `Gneiss-ios.xcconfig`, so the engineer is right that mac/win build double.
- **Include roots:** they cover common-win (`hvoya`, plog) plus Gneiss-win (`hvoya/hvoya`, `hvoya/hvoya/utils`).
- **No reconfigure loop:** configure never writes the files it depends on.

### What I ran vs. only read
**Ran:**
- Docker 24.04, GCC 13.3: configured and built `Gneiss-clap` and `Gneiss-app` from current HEAD (7e3a1c9) against IPlug2-T-003. Both PASS.
- The generated `plug_build_info.hpp` shows `linux-experiment`/`7e3a1c9`, so the HEAD dependency works with loose refs.
- About 70 GCC `-Wstringop-overflow` warnings come from `IParam` atomic loads via `GetParam`. They're GCC false positives, unrelated to this task.
- The engineer's `t003-gneiss` header says 52cb5d6 only because it was built before the fix was committed. It isn't the staleness bug.
- **APP under Xvfb + openbox:** it opens at 940×637 (620 + 17 menu bar) at the right size, which confirms the first-frame notification and the SWELL resize. Toggling MORPH four times alternated 217 and 637 high, with the client origin fixed at 331,420 and the view always 17 px below it: no drift, menu offset kept.
- `git merge-tree hvoya-linux HEAD` is clean.
- `.gitignore`, `build-cmake.sh` and `build-skia-linux.sh` are blob-identical to 1a249af60.

**Read only:** VST3 and REAPER behaviour, stand-in menu rendering, the missing-fontconfig path, 22.04, and the mac build. For those I relied on the engineer's evidence. The mac claim is consistent with the code, since every shared-file change is under `OS_LINUX` or in `hvoya.cmake`.

**Cleanup:** I removed my `rev1-gui` container; `rev1-build` was `--rm`. My only leftover is the gitignored build dir `linux-research/build/rev1-gneiss`, which is safe to delete.
