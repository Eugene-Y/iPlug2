# T-001 — VM verification of P3a/P3b on a real GNOME desktop

- **Status:** in-progress
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
- **Next:** finish these probes, then add a P3b section to notes/14.

## Log
- 2026-10-14: the agent was interrupted twice by limits; its work is preserved in e21ebc5f1 and d0586513d.
