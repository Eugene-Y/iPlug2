# Review of T-008: P3 polish (2026-10-16)

Range reviewed: `hvoya-linux..linux/T-008` (merge base `30580fe02`), commits `d0b8cdcba` … `65e901691`; code in
`IGraphics/Platforms/IGraphicsLinux.{h,cpp}` and a comment in `IGraphicsLinuxFontconfig.h`. Reviewer evidence:
`evidence/t008-review/`, reviewer probe `evidence/t008-review/probe/focus-copy.sh`.

Sources:
- [R]: re-run in Docker (REAPER 7.82 aarch64, Xvfb, openbox, llvmpipe), containers `t008r`/`t008r2`, builds
  `build/t008r` (Release), `build/t008r-trace` (`-DIGRAPHICS_TRACE_LINUX_INPUT`); base = the implementer's
  `build/t008-base` (pre-T-008 code, built at the claim: no `FitMenusOnScreen`/`CheckKeyboardFocus` symbols);
- [E]: the implementer's evidence, screenshots opened by the reviewer;
- [C]: code reading. The VM was not used (fragile today); the VM claims rest on the implementer's evidence.

**Load during the re-runs.** The implementer's containers `t008` and `t008b` each still run an orphaned REAPER with an
open IPlugControls editor (VST3 in `t008`, CLAP in `t008b`), drawing through llvmpipe at ≈2 cores each since about
09:55 container time. They were left alone, as asked; so every reviewer run, base and dev alike, ran with ≈4 cores of
background load on the 6-core colima VM. The coordinator should remove those two containers.

## Re-runs

| Check | Result |
|---|---|
| Build CLAP + VST3 + APP, Release and trace | PASS, no warning from the Linux platform files (`build/t008r.log`, `build/t008r-trace.log`) [R] |
| Full `iplugcontrols-p3b.txt`, CLAP | **FAIL (14)**: at the Ctrl+C step the edit had no keys, Escape reached REAPER and closed the FX window, the rest cascades (`reaper-clap/`) [R] |
| Same, VST3 | **FAIL (13)**: one step earlier, the Escape that should cancel an edit closed the FX window (`reaper-vst3/`) [R] |
| APP script | PASS, no failed check (`app/`) [R] |
| Focus probe (`probe/focus-copy.sh`): double-click the IVNumberBox, 1 s, Ctrl+C, Escape, repeated in one REAPER session (the P3b Ctrl+C step) | Batch 1, one container at a time: base **30/30**, dev Release **17/30**, dev trace **21/30**. Batch 2, base and dev at the same time in two containers (heavier load): base **18/20**, dev **12/20**. Together: base 2/50 failed, dev 30/80. Every failure: Escape closed the FX window, clipboard empty (`focus-copy-{base,dev,trace}-clap{,-2}/`) [R] |
| Base full P3b scripts under the same load | CLAP **FAIL (13)** at the Escape-cancel step (FX window closed), VST3 PASS (`base-reaper-{clap,vst3}/`): the old delay is not immune to this load either [R] |
| L2 steps (`iplugcontrols-l2.txt`), CLAP | PASS; own and stand-in menus reopen within 50 ms of a choice (`l2-clap/l01`, `l02`) [R] |
| `hidden-draws.sh`, CLAP | visible 41 → FX minimized **0** → restored 40 → covered **0** → uncovered 44 Draws / 3 s; `h01`/`h02` fully redrawn (`hidden-clap/`) [R] |
| `menu-edges.sh`, CLAP | the caption dropdown at the bottom edge opens beside the control with all 4 items; right-edge own and stand-in menus open left of the button inside the screen (`menus-clap/e01..e03`) [R] |
| Shared files | Code: Linux files only. But `Dependencies/IPlug/VST3_SDK/README.md` changed (M1) [C] |

## The implementer's evidence, checked

- `menus/docker-base-clap/e02` vs `docker-dev-clap/e02`: base shows one item ("one") cut by the screen edge, dev the
  full four-item menu beside the caption. `vm/menu-edges-clap/e02` (GNOME) shows the same as dev. `docker-dev-clap/e01`:
  the IVMenuButton menu inside the screen. Matches the Log [E].
- `l2/base-clap/l01`: no menu after the second click; `l2/dev-clap/l01`: the menu is open. `l2/base` FAIL ×2, `dev` PASS [E].
- `hidden/docker-base-clap` 74/85 Draws while minimized/covered, `docker-hide-clap` 0/0, `h01-restored`/`h02-uncovered`
  fully drawn. `vm/wm-controls-clap`: main minimized 0 Draws (FX WM_STATE Iconic), FX-only minimize ignored by Mutter
  (WM_STATE stays Normal, 117 Draws), `vm/hidden-draws-clap`: covering does nothing under Mutter. Matches the Log [E].
- `ctx-menu-docker-vst3/m01-menu-open`: REAPER's GTK parameter menu is open over the editor; RESULT: envelope
  created, popup returned after 2.9 s, 0 Draws meanwhile. Matches item 7 [E].
- `vm/fc-variable{,-base}`: Bold of Ubuntu Sans takes the variable file; with the static-bold fixture fontconfig offers
  the fixture first and it is taken. Supports the won't-fix of item 5 [E].
- `vm/focus-edit-clap-trace`: 10/10, trace 10 takes, 0 retries, every check "ours"; `vm/app-focus-click` 12/12 [E].
- `docker-final/`: CLAP, VST3, L2 VST3 PASS; churn ×100 PASS (fds 6/10, sockets 1/3, threads 14/40 equal);
  `threads-clap/input-threads.txt`: every hit tid 1, incl. `CheckKeyboardFocus` 21, `FitMenusOnScreen` 83 [E].
  Note the same census: **57 `TakeKeyboardFocus` for 12 releases**, ≈5 requests per edit or menu under openbox.
- `menus/reaper-controls-clap`: the Log's one "flake" (FAIL 14 at the Ctrl+C step) is the same failure as H1.
- `gdb-crash.txt` files in passing runs hold only the harness's SIGINT when it detaches gdb; no crash anywhere [E].

## Findings

| # | Sev. | Finding | Suggested fix |
|---|---|---|---|
| H1 | **High** | **Focus requested at the release loses a focus fight in REAPER under openbox when the machine is loaded; Escape then closes the FX window mid-edit.** A regression of items 1+2. Probe (above): base lost 2 edits of 50, dev 30 of 80; both full P3b scripts FAIL for the reviewer (base: 1 of 2), and the implementer's own runs had 1 failure in 15 (`menus/reaper-controls-clap`). The load is a factor, and the old 150 ms margin is thin too (`focus-copy-base-clap-2/focus-10.txt`: our delayed request at 0.27 s, GDK's proxy takes it at 0.36 s), but the new scheme fails about 9× as often. Trails: in base the WM focuses the FX top-level, GDK moves it to its proxy (≈100 ms later under this load, not the ≈25 ms of the `kClickFocusSettle` comment), our request lands ≈300 ms after the click and **nothing takes it back**. In dev our request lands *during* that sequence (≈0.1 s), and every request is answered ≈60–130 ms later by the FX top-level taking the focus again (FocusOut NotifyAncestor), often followed by the proxy: 3–6 steals per edit (`focus-copy-dev-clap/focus-2.txt`: ours 0.096, top 0.315, ours 0.395, top 0.489, proxy 0.583, keys lost). The budget is spent by then: 2 retries (`IGraphicsLinux.cpp:1078`, `kMaxFocusRetries`), then one check (`:1149`, `CheckKeyboardFocus`), whose own request (`:1224`) neither re-arms the check nor counts as a request for the retry window, so the steal after it is never answered (trace, trial 4: First, retry, retry, check → host, request, FocusOut NotifyAncestor, nothing; `focus-copy-trace-clap/editor-focus-trace.txt`, `focus-4.txt`). The VM (Mutter) runs did not show it: there GDK answers fast and no steal follows a request. | Do not answer a host's focus change with an immediate counter-request while the click's own focus sequence is still running: that is what keeps the fight going. Keep the timestamped request at the release (it wins outright under Mutter), but when a FocusOut into the host's own top-level follows it, **retry once the focus has stayed in the host's window for `kClickFocusSettle`** (the old 150 ms rule, measured from the last focus change rather than from the release), and bound the whole episode by time since the release (e.g. 1 s) instead of by a count. Let every request, the check's included, go through `TakeKeyboardFocus` so `mFocusTakenAt`/`mFocusChecked` re-arm. Re-run the probe (`probe/focus-copy.sh`, ≥30 trials) under the same background load, plus the VM `focus-edit` and `app-focus` runs. Correct the `kClickFocusSettle` comment (`:77`) |
| M1 | Medium | **`Dependencies/IPlug/VST3_SDK/README.md` is replaced by the downloaded SDK's own README** (302-line diff) in the claim commit `d0b8cdcba`. `Dependencies/` is "don't modify", the file has nothing to do with the task, and it would land in `hvoya-linux` and, from there, the product branch. The Log's "only Linux files changed" misses it | Restore the file from `hvoya-linux` in a fixup commit (snapshot first), and check `git status` for SDK-download side effects before committing |
| L1 | Low | **After a ReparentNotify the watched top-level can be the window manager's frame.** `WatchTopLevel` (`:1634`) takes `FindClientTopLevel`, which falls back to the outermost ancestor below the root when no ancestor has WM_STATE yet. A host that passes its own top-level as the parent and maps it after the open (Carla's X11 plug-in window does) gets a ReparentNotify on `mParent` when the WM frames it, usually before the WM sets WM_STATE: the frame is selected and watched, and minimizing is never seen (drawing goes on, the pre-T-008 behaviour; no stuck state) [C] | When the chosen window has no WM_STATE, re-run `WatchTopLevel` on the next MapNotify/Expose of our window (or watch PropertyNotify on the whole ancestor chain until one gets WM_STATE). Test with a small host that maps its top-level after the open (`scripts/probe/mini-clap-host-scale.cpp` could be adapted) |
| L2 | Low | **`FitMenusOnScreen` overwrites a plug-in's own `SetMaxBounds` for good**, and its comment says "is replaced meanwhile" (`:1527`): the restore writes the whole editor, not the plug-in's value. Also, a menu opened without a click (key, timer, host message) after the editor was moved back on screen keeps the last click's clamp until the next click [C] | IPopupMenuControl has no getter, so without a shared change the value cannot be kept: say "overwrites" in the comment. Optionally refit in `OnDisplayTimer` when the window's position changed (ConfigureNotify) instead of on every button event |
| L3 | Low | `kFocusRetryWindow` has two meanings: the FocusOut retry window and how long an APP click wants the keys (`:1138`). Reads as if the APP's behaviour depended on retries | Its own named constant (e.g. `kAppClickKeepsKeys`) |
| I1 | Info | X round trips per button event: `FitMenusOnScreen` runs on every press **and** release while a popup control exists (IPlugControls always has one): `XTranslateCoordinates` + 2 `XGetWindowProperty` (atoms come from Xlib's cache), even when no menu opens. Small on a local server; noticeable on a remote X | Fit on the press only (and in `CreatePlatformPopupMenu`), or cache the work area and refresh it on root PropertyNotify `_NET_WORKAREA` |
| I2 | Info | Hidden/obscured detection: selecting `PropertyChangeMask` on the host's top-level from our own connection is clean (per-client masks; `mParent` keeps `StructureNotifyMask` through the OR in `watchMask`); BadWindow from a gone top-level is trapped by the following synchronous reads in the same trap. `VisibilityNotify` only works without a compositor (documented). No path leaves drawing stopped: unmap/remap and restore send VisibilityNotify/PropertyNotify and redraw everything [C][R] | — |
| I3 | Info | `FinishFadingMenu` is safe: the completion went out in `CollapseEverything` before the fade, so the forced `OnEndAnimation` stages run no plug-in code (only `UpdateTooltips`), and nothing can close the editor inside it [C] | — |

Lifetime [C]: none of the new paths runs plug-in code except `DeclineKeyboardFocus` → `EndTextEntryOnFocusLoss`
(commit, menu completion), and both callers return right after it; `ProcessXEvents` checks `*open` after every event.
`mScreenFittedPopup` is only compared, never dereferenced. Threads: all new entry points on tid 1 [E].

Code style [C]: Linux files only, 2-space indent, `m`/`p` prefixes, `std::exchange` with `<utility>`; comments are
WHY-only and match the code, apart from `kClickFocusSettle` (H1) and "meanwhile" (L2).

## Verdict

**Accept after fixes.** Items 3–8 are done and verified (menus clamp, minimized/covered drawing under openbox and
minimized under Mutter, L2, won't-fix fonts with evidence, harness and docs). Items 1+2 must be reworked: under
openbox with load the new focus scheme loses about 37 % of edits in the probe, against 4 % for the old delay (H1), so
the acceptance lines "must not regress the REAPER focus results" and "full P3b scripts still PASS in Docker" do not
hold. The settle-based retry suggested in H1 should also cure the old delay's residual losses. M1 must be dropped from the branch before the merge.

## Round 2 (fixes `67b2aff9b`, `48073cd22`, `c9778b586`)

Reviewed: `65e901691..c9778b586`, code in `IGraphicsLinux.{h,cpp}` (from WIP `48073cd22`). Reviewer evidence:
`evidence/t008-review/r2/`. Builds `build/t008r2` (Release) and `build/t008r2-trace`, no warning from the Linux files.
Load: two reviewer containers (`t008r-load1/2`, `probe/load-reaper.sh`), each a REAPER with an open IPlugControls
editor, ≈2.4 cores each for the whole batch: about the round-1 load or a little more. The first T-008 version, re-run
as the control, still fails under it.

| Check (Docker, under that load) | Result |
|---|---|
| Focus probe ×30, first T-008 version (`build/t008r`, control) | **24/30**: the load reproduces round 1's failure (`r2/focus-copy-r1-clap/`) |
| Focus probe ×30, fix Release | **30/30** (`r2/focus-copy-fix-clap/`) |
| Focus probe ×30, fix trace | **30/30**; 54 requests, 24 losses into the host's window, 0 episodes given up, 30 releases; at most **2 requests per edit** (6 edits 1, 24 edits 2), so no fight (`r2/focus-copy-fix-trace-clap/editor-focus-trace.txt`) |
| Full P3b CLAP / VST3 | **PASS / PASS** (`r2/reaper-{clap,vst3}/`; `c10-textentry-os` opened by the reviewer) |
| APP script | no failed check, alive 4/4 (`r2/app/`) |
| M1 | `git diff hvoya-linux...linux/T-008 -- Dependencies` is empty |

### The new state machine [C]

States: `mHoldsFocus` (requested), `mFocusLost` (the host's window took it within the episode), `mFocusDeclined`,
`mFocusChecked`, an episode start (`mFocusEpisodeAt`, 1.5 s) and, while lost, the watched focus window with its
quiet-since time.
- **Bounded:** while lost, a new request goes out only after the focus has stayed on one window for 150 ms, inside a
  1.5 s episode. That allows at most about 10 requests per click, and the trace shows 1–2. Only a user click starts a
  new episode, and past it the host keeps the focus (`mFocusDeclined`), so nothing can fight forever.
- **No stuck state:** a move to another top-level while lost is caught by the polling and ends the edit. Escape,
  Return or a commit releases the focus from both "held" and "lost" (`!wantsKeys && (holds || lost)`). A click clears
  `mFocusDeclined`.
- **Cost:** the polling is one `XGetInputFocus` per event/frame, only while lost (≤1.5 s); `LocateFocus` runs only on
  a change. The 1 s WM_STATE re-check (L1) runs only until a top-level with WM_STATE is found. Fitting the menus on the
  press only removes the release's round trips (I1).
- All requests go through `TakeKeyboardFocus`, so every one re-arms the 150 ms check. That was H1's second point.
- Comments follow the code. The `kClickFocusEpisode` comment now gives the measured ≈0.8 s under openbox, and the
  `FitMenusOnScreen` comment says "overwrites … for good" (L2). L3 is resolved: the APP's click and the retries now
  share one meaning, the episode.

### New findings (round 2)

| # | Sev. | Finding | Suggested fix |
|---|---|---|---|
| R2-L1 | Low | **`mFocusBeforeKnown` and the episode start stay set after a decline.** `ReleaseKeyboardFocus` is the only place that clears them, and it runs only when the focus is held or lost. `DeclineKeyboardFocus` (another top-level took the focus: alt-tab, a click into REAPER's main window, XEmbed FOCUS_OUT) and the expiry branch of `AskForKeyboardFocusAgain` clear both flags, so the next take keeps the earlier edit's restore target (`mFocusBefore`, which may be a destroyed window, and then the BadWindow fallback gives the focus to PointerRoot). If that next take is not a click (an edit opened by a key, a timer or the plug-in), it also keeps the old `mFocusEpisodeAt`: every loss is then outside the episode and is not answered, which is round 1's failure mode for that edit. A click-started edit restarts the episode in `UpdateEditingState`, so the probe and scripts do not show it | In `UpdateEditingState`, when `!wantsKeys && !mHoldsFocus && !mFocusLost`, clear `mFocusBeforeKnown` together with `mFocusDeclined`; or clear it in `DeclineKeyboardFocus` and the expiry branch |
| R2-I1 | Info | `UpdateEditingState` now reaches plug-in code itself: `AskForKeyboardFocusAgain` → `DeclineKeyboardFocus` → `EndTextEntryOnFocusLoss` (commit, menu completion). Afterwards the function still reads `w` (the paste block). `IGEditorDelegate::CloseWindow` deletes the IGraphics, so a plug-in or host that closed the editor from that commit would leave the rest of `UpdateEditingState` running on freed memory. Before round 2 this path ran only from `ProcessXEvents`, behind its `*open` check | Hold `const std::shared_ptr<bool> open = w.mOpen;` and return after `AskForKeyboardFocusAgain` when `!*open`, as the paste block already does |

Open (VM only; the VM was not used in this round either): `vm/probe/focus-edit.sh` CLAP + VST3 and the APP script
with the fix build, as the task file lists. Mutter answers the release request directly (round 1), so the new
lost/quiet path should rarely run there.

### Verdict (round 2)

**Accepted.** H1 is fixed: under the round-1 load the probe went from 24/30 (control) to 30/30 with no focus fight,
and the full P3b scripts (CLAP and VST3) and the APP script pass. M1, L1, L2, L3 and I1 are fixed as described.
R2-L1 and R2-I1 are small and can follow in the next Linux task; neither blocks the merge. The coordinator still has
to do the VM re-checks listed above.
