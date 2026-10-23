#!/usr/bin/env bash
# Builds the Skia static libraries for Linux on the machine's own architecture (x86_64 or aarch64), into
# Dependencies/Build/linux/<arch>/lib. The headers stay in Dependencies/Build/src/skia, as on macOS.
#
# Build on the oldest distribution the plug-ins must run on: the libraries take the glibc symbol versions of the
# build machine's headers. Needs git, python3, clang and ninja.
#
# Usage, from Dependencies/IGraphics: ./build-skia-linux.sh

set -eo pipefail

if [[ $(basename "$PWD") != "IGraphics" ]]; then
  echo "Error: This script must be run from the IGraphics folder."
  exit 1
fi

SKIA_VERSION=chrome/m130
SKIA_URL=https://github.com/google/skia.git

BASE_DIR="$PWD/../Build"
SKIA_SRC_DIR="$BASE_DIR/src/skia"
ARCH=$(uname -m)
OUT_DIR="$BASE_DIR/tmp/skia/linux_$ARCH"
LIB_DIR="$BASE_DIR/linux/$ARCH/lib"

case "$ARCH" in
  x86_64)  TARGET_CPU=x64 ;;
  aarch64) TARGET_CPU=arm64 ;;
  *) echo "Error: unsupported architecture $ARCH"; exit 1 ;;
esac

LIBS=(
  "libskia.a"
  "libskottie.a"
  "libskshaper.a"
  "libsksg.a"
  "libskparagraph.a"
  "libskunicode_icu.a"
  "libskunicode_core.a"
  "libsvg.a"
)

if [ ! -d "$SKIA_SRC_DIR/.git" ]; then
  git clone --depth 1 --branch $SKIA_VERSION $SKIA_URL "$SKIA_SRC_DIR"
fi
CHECKED_OUT=$(git -C "$SKIA_SRC_DIR" rev-parse --abbrev-ref HEAD)
if [ "$CHECKED_OUT" != "$SKIA_VERSION" ]; then
  echo "Error: $SKIA_SRC_DIR has $CHECKED_OUT checked out, not $SKIA_VERSION. Check out $SKIA_VERSION or remove it."
  exit 1
fi

cd "$SKIA_SRC_DIR"
echo "Syncing Deps..."
GIT_SYNC_DEPS_SKIP_EMSDK=1 python3 tools/git-sync-deps
# gn is a native binary of the build machine: fetch it every time, the source tree may be shared by both arches.
python3 bin/fetch-gn

# Like macOS, except:
# - no X11: the Ganesh GL interface is assembled from IGraphicsLinux's GL loader, so Skia never calls GLX itself
#   and libGL stays dlopen'ed;
# - no fontconfig: fonts reach Skia as data from IGraphicsLinux (resources, or its dlopen'ed fontconfig lookup),
#   so libfontconfig does not become a dependency;
# - FreeType is built in; -fPIC because the libraries are linked into shared objects.
./bin/gn gen "$OUT_DIR" --args="
  is_official_build = true
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
  skia_use_egl = false
  skia_use_x11 = false
  skia_use_fontconfig = false
  skia_use_freetype = true
  skia_enable_fontmgr_custom_empty = true
  skia_enable_skottie = true
  skia_enable_svg = true
  skia_enable_pdf = false
  skia_enable_ganesh = true
  skia_enable_graphite = false
  skia_enable_skparagraph = true
  skia_enable_skunicode = true
  skia_enable_tools = false
  cc = \"${CC:-clang}\"
  cxx = \"${CXX:-clang++}\"
  target_cpu = \"$TARGET_CPU\"
  extra_cflags = [\"-fPIC\"]
  extra_cflags_c = [\"-Wno-error\"]
"

ninja -C "$OUT_DIR" skia skottie skshaper sksg skparagraph skunicode_icu skunicode_core svg

mkdir -p "$LIB_DIR"
for lib in "${LIBS[@]}"; do
  cp "$OUT_DIR/$lib" "$LIB_DIR/"
  echo "Copied $lib to $LIB_DIR"
done

echo "Build completed successfully"
