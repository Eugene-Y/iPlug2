#!/usr/bin/env bash
# linux-preset-build.sh <project-dir>...: in the container, cmake --preset linux-debug + build all, then git status.
set -uo pipefail
git config --global --add safe.directory "*"
for P in "$@"; do
  N=$(basename "$P")
  echo "##### $P"
  cd "$P"
  cmake --list-presets
  cmake --preset linux-debug > /tmp/cfg-$N.log 2>&1; echo "configure rc=$?"; grep -E 'Found iPlug2|IPLUG2_DIR' /tmp/cfg-$N.log
  cmake --build --preset linux-debug > /tmp/bld-$N.log 2>&1; rc=$?; echo "build rc=$rc"; [ $rc -ne 0 ] && grep -m5 -E 'error|Error' /tmp/bld-$N.log
  ls -d build-linux/linux-debug/out/$N build-linux/linux-debug/out/$N.clap build-linux/linux-debug/out/$N.vst3 build-linux/linux-debug/out/$N.resources 2>&1
  ls ~/.clap ~/.vst3 2>&1 | tr '\n' ' '; echo
  echo "git status --short:"; git status --short; echo "(end git status)"
  git status --short --ignored | head -5
done
