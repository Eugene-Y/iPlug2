# T-027 — No plug-in names in iPlug2's docs, README, comments and scripts

- **Status:** ready (small; best right before T-015, and again as T-015's last check)
- **Owner:** —
- **Branches/worktrees:** iPlug2 `linux/T-027` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-027`.
- **Depends on:** nothing. T-015 (README) depends on this.
- **Conflicts:** `Documentation/`, `LINUX_PORT_RESEARCH.md`, `CLAUDE.md`; any task that edits docs at the same time.

## The rule (user, 2026-10-18; AGENTS §2)
Plug-ins depend on iPlug2, not the other way round. iPlug2's shipped text — `Documentation/`, `README.md`, code comments, `Scripts/`, `Examples/` — never names a plug-in (Gneiss, Sand, …). A concrete use case is welcome, without the name: "a plug-in built as C++23 with `constexpr` lookup tables" instead of "Sand". A plug-in's own docs may say anything about iPlug2.

Out of scope: commit messages (history is pushed), the `linux-research/` work area, and the user's `hvoya/` utility library (its own product code, not the port's; it names plug-ins in comments — report, don't touch).

## Known hits (2026-10-18, `git grep -i -w -E 'gneiss|sand|melter|dmt|crust|scape'` outside `linux-research/`, `Dependencies/`, `WDL/`)
- `Documentation/linux-development.md:52` — "hvoya's `constexpr` lookup tables (Sand)".
- `LINUX_PORT_RESEARCH.md` — 14 lines. Decide with the user whether this human summary stays in the repo at all (it is a research report in Russian, not a framework doc); if it stays, scrub it.
- `CLAUDE.md:71` (Linux-port section, "Gneiss builds against it") — the port section is working instructions for agents; ask the user whether it stays in the shipped `CLAUDE.md`.
- Code under `IPlug/`, `IGraphics/`, `Scripts/`, `Examples/`: none found.

## Steps
1. Re-run the grep (also catch `Gneiss-`, `Sand.clap` etc. — `-w` treats `-` and `.` as boundaries, so they are covered) and look at `IPlug/Linux`, `IGraphics/Platforms/IGraphicsLinux*`, `Scripts/linux`, `Documentation/` by hand for indirect mentions ("the morph plug-in", product-specific param names, paths like `hvoya/Gneiss`).
2. Rewrite each hit as a use case without the name. Keep the technical content.
3. Add a check the coordinator can run before every merge: `linux-research/scripts/coord/check-product-names.sh` (exit non-zero on a hit outside the allowed paths).
4. Independent reviewer (AGENTS §4), then hand over.

## Acceptance
- The grep above returns nothing outside the out-of-scope paths (or only lines the user explicitly kept).
- The check script exists and the coordinator's merge procedure (`COORDINATOR.md`) calls it.

## Log
