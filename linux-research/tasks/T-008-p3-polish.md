# T-008 — P3 polish (findings from the VM run and reviews)

- **Status:** in-progress (review fixes)
- **Owner:** T-008 worker (Claude, 2026-10-16)
- **Branch/worktree:** `linux/T-008` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-008`.
- **Depends on:** T-002 (merged).
- **Conflicts:** `IGraphics/Platforms/IGraphicsLinux*`, also touched by T-007 — don't run them in parallel.

## Items
Details and repro steps are in `notes/14-vm-p3a.md` (Parts 1–2), `notes/15-review-p3b.md`, `notes/13`. Each item gets its own commit.
1. **P3b-V1 — APP focus-on-click race.** In the standalone APP, a key right after a click can go to the SWELL window instead of the editor; under Mutter+GDK with a same-millisecond synthetic click, Escape closed the APP in 4 of 10 runs. Make the APP's editor take focus reliably.
2. **Timestamped `XSetInputFocus`** (notes/15 option 1; the notes/14 probe showed a request at button release wins with either timestamp). Replace or augment the 150 ms delayed focus grab. Keep the delay as a fallback only if needed. Verify in Docker (openbox) and on the VM (Mutter). Must not regress the REAPER focus results: 10/10 in notes/14.
3. **P3b-V2 — menus clamped to the editor, not the screen.** With the editor partly off-screen, a dropdown opens off-screen. At minimum, clamp the menu to the intersection of the editor and the screen (`XGetGeometry` of the root, `_NET_WORKAREA`). Optional, as a stretch: draw IGraphics menus in an override-redirect popup window so they can extend beyond the editor — evaluate the cost first and document it, but don't implement it without a note in the Log.
4. **P3a-V2 — the editor draws while the host is minimized.** GNOME leaves our window mapped. Stop or throttle drawing when the host's top-level is iconified (`_NET_WM_STATE_HIDDEN` / `WM_STATE` on the top-level ancestor) or fully obscured (`VisibilityNotify`).
5. **P3b-V4 — variable-font bold.** `LoadPlatformFont` by name with Bold on a variable family (Ubuntu Sans) returns regular. Either choose a static bold face if present, or document it as a limitation (stb_truetype cannot apply variation axes).
6. **P3b-V5 — harness:** narrow the flaky `expect-same` rect in the APP steps to `890 410 90 40`.
7. **P3b-V3 — docs:** correct notes/13 finding 5. The REAPER VST3 context menu does appear with a held right click (notes/14).
8. **L2** (notes/15) — a menu requested while the previous one is still collapsing is ignored. Decide: fix (queue, or finish the collapse immediately) or keep documented.

## Acceptance
- [x] Each item: fixed with evidence (Docker and, for 1–4, the VM), or explicitly documented as won't-fix with a reason.
- [x] Full P3b step scripts still PASS (CLAP, VST3, APP) in Docker and on the VM; main-thread check; churn 100×.
- [x] Self-review; evidence in `evidence/t008/`; Log complete.

## Log

- 2026-10-16: claimed; worktree `/Volumes/Storage/code/IPlug2-T-008`, branch `linux/T-008` from `30580fe02`. Container `t008`, build dirs `build/t008-*`.
- 2026-10-16: all items done; results below. Only Linux files changed in code (`IGraphicsLinux.{h,cpp}`,
  a comment in `IGraphicsLinuxFontconfig.h`); no shared file, so no macOS build was needed. The rest is harness
  and notes.

### Results per item ([D] Docker openbox/llvmpipe, containers `t008`/`t008b`; [V] VM GNOME/Mutter/virgl)

| # | Commit | Outcome | Evidence (`evidence/t008/`) |
|---|---|---|---|
| 6 P3b-V5 | 4386ec8c0 | All number-box checks use `890 410 90 40` (the box only; the expect-changes too, which the animation could pass falsely) | [V] 10 + 3 APP runs: `same a03→a04` every time (`vm/app-script-*`, `vm/app-final-*`) |
| 7 P3b-V3 | b9d62c891 | notes/13 finding 5 rewritten: the menu shows with a held right click; `popup()` is a nested loop, 0 Draws meanwhile | [D] `ctx-menu-docker-vst3/` (menu open, envelope created, popup 2.9 s) |
| 1+2 | c0d9cfb67 | One mechanism, one commit: focus requested at the click's release **with the release timestamp**; FocusOut into the host's own top-level (incl. GDK's proxy) within 500 ms → retry with CurrentTime (≤2); one check 150 ms after each request (the old delay, now only a fallback); every click while keys are wanted asks again (openbox refocuses the top-level on every click); restore target = the host's own focus window. APP: a click runs the same request (was a lone CurrentTime request) | [D] APP + CLAP + VST3 P3b PASS (`focus/`, `docker-final/`). [V] `focus-edit` 10/10 CLAP (trace: 10 takes, 0 retries, every check "ours", focus given back to the GDK proxy), 10/10 VST3, 10/10 with 0-s holds; `app-focus` same-ms click 12/12 (base 12/12 this time, notes/14 had 1/12); full APP script **13/13** with no failed check (notes/14: Escape closed the APP 4/10) |
| 3 P3b-V2 | bd5bb3df2 | Before a click and before a stand-in menu, the popup control's max bounds = editor ∩ work area (`_NET_WORKAREA`, else root) when the editor is partly off screen; back to the whole editor otherwise | [D] `menus/docker-{base,dev}-clap/e02`: base 1 item visible, now 4 beside the control; [V] `vm/menu-edges-clap/e02` same |
| 4 P3a-V2 | e2052ad83 | Watch the host's client top-level (WM_STATE ancestor) for Iconic / `_NET_WM_STATE_HIDDEN`, and our window for VisibilityFullyObscured; IsDirty keeps running, only Draw+swap stop; full redraw when shown | [D] `hidden/`: FX minimized 74 → 0 Draws/3 s, covered 85 → 0, restore/uncover redraws (h01/h02). [V] `vm/wm-controls-clap`: REAPER main minimized **0** Draws/4 s (notes/14: 109–113); Mutter ignores minimizing the FX window itself; covering does not stop drawing under Mutter (composited: no VisibilityNotify), `vm/hidden-draws-clap` |
| 8 L2 | c935d5f9d | A button press while a popup control is past its completion (flickering/collapsing/idling) advances it to collapsed first (`OnEndAnimation` per stage) and redraws: the click's menu opens. Requests without a click during the fade stay ignored (as mac/win) | `steps/iplugcontrols-l2.txt` (new `click-click` xact step): [D] base FAIL ×2 (own and stand-in menu not reopened, `l2/base-clap/l01`), now PASS CLAP + VST3; [V] base FAIL ×2, now PASS |
| 5 P3b-V4 | 300e4f6a6 | **Won't fix**, documented (notes/13 known issues, header comment). A same-family static-face reorder was written and measured, then dropped: fontconfig already ranks such a face first (old code too); without one the variable default (regular) is kept, family over weight. stb_truetype cannot apply variation axes | [V] `vm/fc-variable{,-base}/` (fixture: a static bold renamed into "Ubuntu Sans") |

### Acceptance re-runs on the final code (`build/t008-final`)
- [D] Full P3b scripts: APP, CLAP, VST3 PASS (`docker-final/`); L2 VST3 PASS; `EDIT=1` churn 100× CLAP + VST3 PASS
  (fds 6/10, sockets 1/3, threads 14/42 before = after); thread census during the CLAP script: every hit tid 1,
  incl. the new `CheckKeyboardFocus`, `FitMenusOnScreen` (`docker-final/threads-clap/`).
- [V] Full P3b scripts CLAP + VST3 PASS (`vm/reaper-controls-*`, all Draw/ProcessXEvents tid 1), APP ×3 PASS,
  L2 PASS, menus, minimized drawing as above. The VM's virgl GPU hung once during batch 2 (user restarted it);
  the APP runs were redone after the reboot (`vm/app-final-*`); the VM input-path census was not redone (Docker's
  covers the same code paths).
- Flake watch: one Docker CLAP P3b run (items 1–3 build) failed at the Ctrl+C step (keys lost, cascade);
  14 further CLAP runs on the same and later builds passed (`flaky/`), base 4/4. Not reproduced with the trace
  build (6/6). Keep an eye on it in the next merges.

### Stretch (item 3): IGraphics menus in an override-redirect popup window — evaluated, not implemented
Cost: a second X window with its own GL drawable (or a second IGraphics), drawing `IPopupMenuControl` outside the
editor's coordinate space, a pointer grab to see clicks outside, keyboard routing, and the menu's lifetime tied to
two windows; essentially a native menu toolkit. Several days and new failure modes (grabs in hosts) for what the
on-screen clamp already makes usable. Left for a later decision.

### Cross-platform findings (report, not fixed here)
| Sev. | Finding | Where |
|---|---|---|
| Low | L2 on mac/win with a plug-in-attached `IPopupMenuControl`: a menu requested during the previous one's ≈300 ms fade is ignored; the owner keeps `mInPopupMenu` and its host gesture until the next menu. Repro: `steps/iplugcontrols-l2.txt` logic (choose, reopen within 50 ms) | `IPopupMenuControl.cpp:456` (`CreatePopupMenu` expands only from kCollapsed) |
| Low | A menu opened from inside `OnPopupMenuSelection` with an async popup control loses its owner: `SetControlValueAfterPopupMenu` sets `mInPopupMenu = nullptr` after the callback, so the nested menu's completion and gesture end go nowhere | `IGraphics.cpp:279–301` |
| Info | `IPopupMenuControl::mMaxBounds` is set once (first menu) and never follows a later UI resize | `IPopupMenuControl.cpp:453` |

### Review round 1 (notes/19): fixes
| # | Fix | Evidence |
|---|---|---|
| H1 | Focus rework (code in WIP 48073cd22, kept by the coordinator over a limit): every click/take begins a 1.5 s episode (`kClickFocusEpisode`); a FocusOut into the host's window, or the check 150 ms after any request, marks the focus lost; it is asked for again (CurrentTime) only once the focus has stayed put 150 ms (`kFocusQuiet`, polled with `XGetInputFocus` while lost, `LocateFocus` only on a change); a move to another top-level meanwhile ends the edit; past the episode the host keeps it. All requests go through `TakeKeyboardFocus`, so every one re-arms the check. No retry count any more | Docker, load = 1–2 extra REAPERs with open editors (≈3.4–4.7 cores): reviewer's probe `scripts/probe/focus-copy.sh` ×30 — heavier load: first T-008 version **18/30**, fix **30/30**, base 30/30, fix trace 30/30 (57 requests, 27 losses, 0 given up, `focus-load/*-load2/`); lighter load: fix 30/30, base 30/30, first version 29/30. Under the heavier load: full P3b CLAP ×2, VST3 ×2, APP, L2 CLAP + VST3 PASS; churn `EDIT=1` ×100 CLAP + VST3 PASS; hidden-draws 0 Draws minimized/covered (`review-fix/`) |
| M1 | `Dependencies/IPlug/VST3_SDK/README.md` restored (67b2aff9b); `install-sdks.sh` overwrites it, so check `git status` after it | `git diff hvoya-linux -- Dependencies` empty |
| L1 | The watched top-level remembers whether it has WM_STATE; without, it is looked up again once a second from the frame timer until one does (a host that maps its window after the open) | code; not exercised by REAPER (WM_STATE present at open) |
| L2 | `FitMenusOnScreen`'s comment says it overwrites a plug-in's own `SetMaxBounds` and that a menu opened without a press keeps the last press's limit | — |
| L3 | `kFocusRetryWindow` is gone; the APP's click and the retries share one meaning now: the click's focus episode | — |
| I1 | Menus are fitted on the press only (and for stand-ins), not on the release | menu-edges / L2 PASS |

Open, VM-only (the VM's GPU hung twice today; for the coordinator, light and short): `vm/probe/focus-edit.sh` CLAP
+ VST3 ×10 and the APP P3b script ×3 with the fix build (the earlier VM results are for the first focus version).
