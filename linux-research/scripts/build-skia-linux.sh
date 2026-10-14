#!/usr/bin/env bash
# Build Skia (static) for Linux, native arch, with Ganesh GL/GLX + fontconfig, matching the
# chrome/m130 branch iPlug2's Dependencies/IGraphics/download-igraphics-libs.sh pins.
# Runs INSIDE the container. Output: cache/skia/<arch>/{lib,include-root}
# Usage: build-skia-linux.sh [skia-branch]
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
BRANCH="${1:-chrome/m130}"
ARCH="$(uname -m)"
SRC="$HERE/cache/skia/src-${BRANCH//\//-}"
OUT="$HERE/cache/skia/out-$ARCH"
DEST="$HERE/cache/skia/$ARCH"
case "$ARCH" in aarch64) CPU=arm64;; x86_64) CPU=x64;; *) CPU="$ARCH";; esac

[ -d "$SRC/.git" ] || git clone --depth 1 --branch "$BRANCH" https://github.com/google/skia.git "$SRC"
cd "$SRC"
# git-sync-deps pulls every DEPS entry (dawn, emsdk, ...); only what the GN args below need is used.
GIT_SYNC_DEPS_SKIP_EMSDK=1 python3 tools/git-sync-deps
python3 bin/fetch-gn
./bin/gn gen "$OUT" --args="
  is_official_build = true
  is_debug = false
  target_cpu = \"$CPU\"
  cc = \"clang\"
  cxx = \"clang++\"
  skia_use_system_libjpeg_turbo = false
  skia_use_system_libpng = false
  skia_use_system_zlib = false
  skia_use_system_expat = false
  skia_use_system_icu = false
  skia_use_system_harfbuzz = false
  skia_use_system_freetype2 = false
  skia_use_libwebp_decode = false
  skia_use_libwebp_encode = false
  skia_use_xps = false
  skia_use_dng_sdk = false
  skia_use_expat = true
  skia_use_icu = true
  skia_use_dawn = false
  skia_use_vulkan = false
  skia_use_gl = true
  skia_use_x11 = true
  skia_use_fontconfig = true
  skia_use_freetype = true
  skia_enable_fontmgr_fontconfig = true
  skia_enable_skottie = true
  skia_enable_svg = true
  skia_enable_pdf = false
  skia_enable_ganesh = true
  skia_enable_graphite = false
  skia_enable_skparagraph = true
  skia_enable_skunicode = true
  skia_enable_tools = false
  extra_cflags = [\"-fPIC\", \"-fvisibility=hidden\"]
  extra_cflags_c = [\"-Wno-error\"]
"
ninja -C "$OUT" skia skottie skshaper sksg skparagraph skunicode_icu skunicode_core svg
mkdir -p "$DEST/lib"
for l in libskia.a libskottie.a libskshaper.a libsksg.a libskparagraph.a libskunicode_icu.a libskunicode_core.a libsvg.a; do
  cp "$OUT/$l" "$DEST/lib/" 2>/dev/null || echo "missing $l"
done
ln -sfn "$SRC" "$DEST/src"
echo "Skia $BRANCH built for $ARCH -> $DEST"
