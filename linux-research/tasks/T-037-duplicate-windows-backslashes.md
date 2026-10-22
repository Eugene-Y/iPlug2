# T-037 — `duplicate.py` on Windows writes backslash paths into mac/iOS/web project files

- **Status:** todo (low; ticket from the T-011 review, user 2026-10-22)
- **Owner:** —
- **Branch/worktree:** `linux/T-037` from `hvoya-linux`.
- **Depends on:** T-011 (merged 839e1b51d).
- **Conflicts:** `Examples/duplicate.py` (shared with mac/win).

## Problem (pre-existing upstream code, 2018; details in `notes/29-review-T-011.md`, round 2)
The `config/`/`projects/` rewrite takes `newroot` from `os.path.relpath`, which gives `..\..` on Windows, and writes it
into every file that held `../../..`: mac/iOS xcconfig and pbxproj, the web/wasm `.mk` files, `makedist-*.sh`,
`prepare_resources-*.py`. When iPlug2 is not a direct child of the target folder, the second replacement matches its
own output and the path doubles (`IPLUG2_ROOT = ..\..\..\iPlug2\iPlug2`). Windows builds are fine (`.props`,
`.vcxproj`); a project duplicated on Windows does not build on mac, iOS or web.

## Fix
Normalise `newroot` to `/` before `dirwalk()` (as T-011 does for the project-folder roots); check that `.props` and
`.vcxproj` still get the form VS expects; check the doubling case.

## Acceptance
- [ ] `ntpath` simulation (as in `evidence/t011/duplicate-windows-ntpath-sim.txt`): every rewritten mac/iOS/web file
  gets `/` paths; no doubling with iPlug2 not a direct child of the target.
- [ ] mac before/after for all examples in- and out-of-tree: no change.
- [ ] Ideally a real Windows run (the user's PC).
- [ ] Independent review; CROSS_PLATFORM_FINDINGS T11-4 updated.

## Log
