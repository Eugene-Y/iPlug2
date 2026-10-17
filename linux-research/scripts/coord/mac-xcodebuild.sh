#!/usr/bin/env bash
# mac-xcodebuild.sh <tree> <project-basename> <out> <scheme>: command-line xcodebuild, every install/build path in <out>.
set -uo pipefail
T="$1"; P="$2"; OUT="$3"; SCHEME="$4"; I="$OUT/inst"; mkdir -p "$OUT"
cd "$T/projects"
xcodebuild -project "$P-macOS.xcodeproj" -scheme "$SCHEME" -configuration Debug -derivedDataPath "$OUT/dd" \
  VST2_PATH="$I/VST" VST3_PATH="$I/VST3" CLAP_PATH="$I/CLAP" AU_PATH="$I/Components" AAX_PATH="$I/AAX" \
  APP_PATH="$I/Apps" REAPER_EXT_PATH="$I/ext" SYMROOT="$OUT/sym" OBJROOT="$OUT/obj" build > "$OUT/xcb-$SCHEME.log" 2>&1
echo "$P $SCHEME exit=$?"; grep -m3 "error:" "$OUT/xcb-$SCHEME.log"
