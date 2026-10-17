#!/usr/bin/env bash
# Command-line xcodebuild of Sand with every install/build location in <out> (nothing in ~/Library or ~/Applications).
# Usage: xcb-sand.sh <sand-tree> <out> <scheme> [configuration=Debug] [extra xcodebuild args...]
set -uo pipefail
SAND="$1"; OUT="$2"; SCHEME="$3"; CONF="${4:-Debug}"; shift 4 2>/dev/null || shift $#
mkdir -p "$OUT"
I="$OUT/inst"
OVR=(VST2_PATH="$I/VST" VST3_PATH="$I/VST3" CLAP_PATH="$I/CLAP" AU_PATH="$I/Components" AAX_PATH="$I/AAX"
     APP_PATH="$I/Apps" REAPER_EXT_PATH="$I/ext" SYMROOT="$OUT/sym" OBJROOT="$OUT/obj")
cd "$SAND/projects"
xcodebuild -project Sand-macOS.xcodeproj -scheme "$SCHEME" -configuration "$CONF" \
  -derivedDataPath "$OUT/dd" "${OVR[@]}" "$@" build > "$OUT/xcb-$SCHEME.log" 2>&1
rc=$?
echo "xcodebuild $SCHEME $CONF exit=$rc"
grep -m5 "error:" "$OUT/xcb-$SCHEME.log"
exit $rc
