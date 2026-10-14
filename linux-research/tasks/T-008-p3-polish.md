# T-008 — P3 polish (findings from the VM run and reviews)

- **Status:** ready
- **Owner:** —
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
- [ ] Each item: fixed with evidence (Docker and, for 1–4, the VM), or explicitly documented as won't-fix with a reason.
- [ ] Full P3b step scripts still PASS (CLAP, VST3, APP) in Docker and on the VM; main-thread check; churn 100×.
- [ ] Self-review; evidence in `evidence/t008/`; Log complete.

## Log
