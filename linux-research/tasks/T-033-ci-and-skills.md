# T-033 — Linux in CI and in the repo's skills

- **Status:** ready (found by the T-016 review, M3)
- **Owner:** —
- **Conflicts:** `.github/workflows/cmake-ci.yml`, `.claude/skills/*`.

## Goal
- `.github/workflows/cmake-ci.yml`: a native Linux job (ubuntu-22.04 and/or 24.04, x86_64; arm64 runner if available) that installs the build deps (`Documentation/linux-development.md` §1), builds the examples' CLAP/VST3/APP with CMake and runs clap-validator on IPlugEffect. Uploading this to GitHub runs on the user's account: the workflow change is a proposal until the user agrees.
- `.claude/skills/` (`validate`, `screenshot`, `build-cmake`, `setup-deps`, …): Linux instructions where the skill is platform-specific (validators, screenshots under Xvfb, CMake presets, SDK setup). No product names (AGENTS §2).

## Acceptance
- The workflow runs green on a fork or locally with `act` (if available); skills checked by following them on Linux.
- Independent review.
