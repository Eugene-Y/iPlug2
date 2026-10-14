# T-001 — VM verification of P3a/P3b on a real GNOME desktop

- **Status:** review-done
- **Owner:** coordinator
- **Branch:** `hvoya-linux` (evidence/notes only, no source changes)
- **Depends on:** —
- **Uses:** the VM (take the lock)

## Goal

Run the P3a/P3b checks on the VM (GNOME/Mutter X11, virgl). These are things Xvfb cannot show:
- real WM focus;
- compositor stacking;
- swap interval;
- clipboard with GNOME apps;
- theme cursors;
- menus near screen edges;
- REAPER's VST3 context menu behaviour.

Report defects with repro steps; don't edit sources.

## State
- **P3a done:** `notes/14-vm-p3a.md` (commit be91aa884).
- **P3b in progress:** probes are in `scripts/vm/probe/`:
  - `focus-edit.sh`, `focus-ts.sh` (timestamped-focus observation);
  - `gnome-clip.sh`, `clip-owner.c`;
  - `menu-edges.sh`;
  - `ctx-menu.sh`/`ctx-close.sh` (REAPER VST3 context menu: does it block, does the editor keep drawing).

  Partial evidence is in `evidence/vm-p3b/` (WIP commit d0586513d).
- **P3b done:** notes/14 Part 2 (results, issues P3b-V1..V6, harness notes); evidence `evidence/vm-p3b/`.

## Log
- 2026-10-14: the agent was interrupted twice by limits; its work is preserved in e21ebc5f1 and d0586513d.
- 2026-10-14: P3b probes finished on the VM (HEAD 9c586203b, `wt/vm-head`, `build/vm-head`, `build/vm-head-trace`):
  full P3b script CLAP+VST3 PASS; P3b paths all on tid 1; delayed focus take 10/10 CLAP + 10/10 VST3 under Mutter
  (no retries); timestamped-focus observation (`focus-ts-probe.c`); GNOME clipboard both ways PASS, GNOME Shell keeps
  the text after the editor closes; menus near the bottom screen edge are cut off; REAPER's VST3 context menu shows
  with a held right click and `popup()` is a nested loop (editor frozen, host actions deferred); variable fonts lose
  bold; APP's final Escape reached SWELL in 4/10 runs (same-millisecond synthetic click vs. Mutter+GDK focus).
  notes/14 Part 2 written; `scripts/vm/README.md` lists the P3b probes and new gotchas (same-ms clicks, xclip holding
  pipes, focuswatch's `atof`, gnome-text-editor drafts). `vm-x.sh` no longer waits on leftover daemons.
- Self-review (fresh pass over notes/14 Part 2 against the Goal list and the evidence): every Goal item is covered
  (focus, stacking/swap in Part 1, clipboard, cursors, edge menus, context menu); corrected three counts/claims
  (context-menu hold times, trace-run count 5 not 6, variable fonts lose bold only). Verdict: complete.
- Open issues for owners: P3b-V1 (APP focus-on-click race, Low–Med), P3b-V2 (menus clamped to the editor, not the
  screen), P3b-V4 (variable-font bold), P3b-V5 (flaky `expect-same` rect in the APP steps), P3b-V3 (correct notes/13
  finding 5); from Part 1: V2 (editor draws while minimized), V3 (virgl per-context leak, driver).
- Left in place: worktrees `linux-research/wt/vm-p3a`, `wt/vm-head` and build dirs `build/vm-p3a*`, `build/vm-head*`
  (git-ignored). No containers running; VM desktop left empty; no VM lock held.
