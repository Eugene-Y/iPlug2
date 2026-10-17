#!/usr/bin/env bash
# Linux CLAP + VST3 build of Sand (or another hvoya product with a Linux CMakeLists) in Docker, from the HOST.
# Recreates T-024's lost build-sand.sh (git-ignored by build-*, so never committed).
# Usage: sand-build.sh <product-tree> <builddir> [release|asan] [target ...]
#   release: Release; asan: RelWithDebInfo + -fsanitize=address (run it with LD_PRELOAD=$(gcc -print-file-name=libasan.so),
#   as evidence/t024/run-checks.sh does). Targets default to <Name>-clap <Name>-vst3, Name = the tree's basename
#   up to the first '-' (Sand-t026 -> Sand).
# Env: IPLUG2_DIR (default: this iPlug2 tree), IMAGE (dock.sh's default image).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
TREE="$(cd "$1" && pwd)"; BUILD="$2"; MODE="${3:-release}"; shift $(( $# < 3 ? $# : 3 ))
NAME="$(basename "$TREE")"; NAME="${NAME%%-*}"
TARGETS=("$@"); [ ${#TARGETS[@]} -eq 0 ] && TARGETS=("$NAME-clap" "$NAME-vst3")
IPLUG2="${IPLUG2_DIR:-$(cd "$HERE/../.." && pwd)}"
case "$MODE" in
  release) TYPE=Release; SAN="" ;;
  asan)    TYPE=RelWithDebInfo
           SAN="CFLAGS='-fsanitize=address -fno-omit-frame-pointer' CXXFLAGS='-fsanitize=address -fno-omit-frame-pointer' LDFLAGS=-fsanitize=address" ;;
  *) echo "mode must be release or asan" >&2; exit 2 ;;
esac
# A worktree's .git points into the main repo's git dir: mount that too (read-only), so the build-info step sees git.
GITCOMMON="$(git -C "$TREE" rev-parse --path-format=absolute --git-common-dir)"
case "$GITCOMMON" in "$TREE"/*) MOUNTS="$TREE" ;; *) MOUNTS="$TREE $GITCOMMON:ro" ;; esac
# Sanitizer flags go in CFLAGS/CXXFLAGS/LDFLAGS (CMake reads them at the first configure of a build dir):
# build-cmake.sh word-splits EXTRA_CMAKE_ARGS, and DOCK_ENV splits on spaces, so neither carries "-a -b".
DOCK_MOUNTS="$MOUNTS" "$HERE/dock.sh" -- bash -c \
  "$SAN BUILD_TYPE=$TYPE EXTRA_CMAKE_ARGS=-DIPLUG2_DIR=$IPLUG2 scripts/build-cmake.sh '$TREE' '$BUILD' ${TARGETS[*]}"
