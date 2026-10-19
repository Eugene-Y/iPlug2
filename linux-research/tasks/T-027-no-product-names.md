# T-027 — No plug-in names in iPlug2's docs, README, comments and scripts

- **Status:** review next
- **Owner:** coordinator (chat f91df715), lock `.linux-port-locks/T-027`
- **Branches/worktrees:** iPlug2 `linux/T-027` from `hvoya-linux`, in `/Volumes/Storage/code/IPlug2-T-027`.
- **Depends on:** nothing. T-015 (README) depends on this.
- **Conflicts:** `Documentation/`, `LINUX_PORT_RESEARCH.md`, `CLAUDE.md`; any task that edits docs at the same time.

## The rule (user, 2026-10-18; AGENTS §2)
Plug-ins depend on iPlug2, not the other way round. iPlug2's shipped text — `Documentation/`, `README.md`, code comments, `Scripts/`, `Examples/` — never names a plug-in (Gneiss, Sand, …). A concrete use case is welcome, without the name: "a plug-in built as C++23 with `constexpr` lookup tables" instead of "Sand". A plug-in's own docs may say anything about iPlug2.

Out of scope: commit messages (history is pushed), the `linux-research/` work area, and the user's `hvoya/` utility library (its own product code, not the port's; it names plug-ins in comments — report, don't touch).

## Known hits (2026-10-18, `git grep -i -w -E 'gneiss|sand|melter|dmt|crust|scape'` outside `linux-research/`, `Dependencies/`, `WDL/`)
- `Documentation/linux-development.md:52` — "hvoya's `constexpr` lookup tables (Sand)".
- `LINUX_PORT_RESEARCH.md` — 14 lines. **User (2026-10-19): keep the research docs, inside `linux-research/`** — move this file there (it then falls under the work area, not the shipped docs).
- `CLAUDE.md:71` (Linux-port section, "Gneiss builds against it") — **user (2026-10-19): remove the section when the port work is finished**, not now; but keep a short note that the Linux port was done by this fork's authors, not by iPlug2's maintainers, so nobody mistakes it for upstream work. Until then, take the product name out of it.
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

### 2026-10-19 — done
- `LINUX_PORT_RESEARCH.md` moved to `linux-research/` (user's decision); its link to the work area fixed; `AGENTS.md` and `PLAN.md` point to the new path.
- `Documentation/linux-development.md`: the clangd note says "projects built as C++23 with `constexpr` lookup tables" (no product, no utility-library name); new paragraph near the top: Linux support in this fork is the fork's work, not iPlug2's maintainers' — report Linux problems to the fork.
- `CLAUDE.md` port section: "the fork's plug-ins build against it" instead of the product name, plus "The Linux port is this fork's own work, not upstream iPlug2's." The section itself goes when the port is finished (user); the authorship note stays then (in `Documentation/linux-development.md` it already does).
- Hand search of `IPlug/Linux`, `IGraphics/Platforms/IGraphicsLinux*`, `Scripts/linux`, `Documentation/` for indirect mentions (morph, USER tab, `hvoya/` paths): none.
- Not changed: "HvoyaAudio" in `Examples/IPlugEffect` (the fork's manufacturer name in its example config, on `hvoya` before the port; not a plug-in name). Out of scope as before: `linux-research/`, `hvoya/`, commit messages.
- `linux-research/scripts/coord/check-product-names.sh [rev]`: `git grep -w -i` of the names in the tree at `rev` outside `linux-research`, `Dependencies`, `WDL`, `hvoya`; exit 1 on a hit. On this branch: clean; on hvoya-linux before: the hits above. `COORDINATOR.md`'s merge procedure runs it (step 3).
