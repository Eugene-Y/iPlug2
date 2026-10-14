#!/usr/bin/env bash
# Configure + build an iPlug2 worktree that has a top-level CMakeLists (upstream-style CMake).
# Runs INSIDE the container (wrap with dock.sh).
# Usage: build-cmake.sh <worktree> <builddir> [target ...]
#   targets default: IPlugEffect-{app,vst3,clap} IPlugControls-{app,vst3,clap}
# Env: BUILD_TYPE (Release), CC/CXX, EXTRA_CMAKE_ARGS, KEEP_GOING=1 (ninja -k 0)
set -uo pipefail
WT="$(cd "$1" && pwd)"; BUILD="$2"; shift 2
TARGETS=("$@")
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(IPlugEffect-app IPlugEffect-vst3 IPlugEffect-clap
                                     IPlugControls-app IPlugControls-vst3 IPlugControls-clap)
mkdir -p "$BUILD"
cmake -S "$WT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE:-Release}" \
      -DIPLUG_DEPLOY_PLUGINS=OFF -DIPLUG2_DISABLE_DEPRECATION_WARNINGS=ON ${EXTRA_CMAKE_ARGS:-} \
  || { echo "CONFIGURE FAILED"; exit 2; }
rc=0
for t in "${TARGETS[@]}"; do
  echo "=== BUILD $t"
  if cmake --build "$BUILD" --target "$t" -- ${KEEP_GOING:+-k 0}; then
    echo "=== RESULT $t PASS"
  else
    echo "=== RESULT $t FAIL"; rc=1
  fi
done
exit $rc
