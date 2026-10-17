#!/usr/bin/env bash
# Builds the Release CLAP / VST3 / standalone of an iPlug2 CMake project for Linux and packs what a
# customer downloads: <Name>_v<version>_linux_<arch>[_<variant>].tar.gz (plug-ins + resources, install.sh,
# README-Linux.txt, docs), plus a separate <same>_debug-symbols.tar.gz that stays with the developer.
#
#   Scripts/linux/package.sh <project-dir> [--arch aarch64|x86_64]... [--variant NAME[:CMAKE_ARGS]]...
#                            [--doc FILE]... [--icon FILE] [--formats "clap vst3 app"] [--out DIR]
#                            [--glibc-max 2.35] [--clean] [--keep-going]
#
# On Linux it builds for the host architecture with the host toolchain. On any other host, or for another
# architecture, it runs itself in Docker (image per arch: $IPLUG_LINUX_IMAGE_<arch>, default
# iplug2-linux:22.04-gcc13[-amd64], linux-research/docker). Without --arch: the host arch on Linux, both
# arches elsewhere.
#
# The oldest glibc the binaries need is the build machine's (glibc versions its symbols); customers need at
# least that. Build on Ubuntu 22.04 for glibc 2.35, which is why the check fails above --glibc-max.

set -euo pipefail

IPLUG2_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SELF="$IPLUG2_DIR/Scripts/linux/package.sh"

die() { echo "package.sh: $*" >&2; exit 1; }

PROJECT_DIR=""
ARCHS=()
VARIANTS=()
DOCS=()
ICON=""
FORMATS="clap vst3 app"
OUT=""
GLIBC_MAX="2.35"
KEEP_GOING=0
CLEAN=0
IN_CONTAINER=0

needs_value() { [ $# -ge 2 ] || die "$1 needs a value"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --arch|--variant|--doc|--icon|--formats|--out|--glibc-max) needs_value "$@" ;;
  esac
  case "$1" in
    --arch)       ARCHS+=("$2"); shift ;;
    --variant)    VARIANTS+=("$2"); shift ;;
    --doc)        DOCS+=("$2"); shift ;;
    --icon)       ICON="$2"; shift ;;
    --formats)    FORMATS="$2"; shift ;;
    --out)        OUT="$2"; shift ;;
    --glibc-max)  GLIBC_MAX="$2"; shift ;;
    --keep-going) KEEP_GOING=1 ;;
    --clean)      CLEAN=1 ;;
    --in-container) IN_CONTAINER=1 ;;
    -h|--help)    sed -n '2,17p' "$SELF" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*)           die "unknown option $1 (see --help)" ;;
    *)            [ -z "$PROJECT_DIR" ] || die "one project dir only"; PROJECT_DIR="$1" ;;
  esac
  shift
done

[ -n "$PROJECT_DIR" ] || die "usage: package.sh <project-dir> [options] (see --help)"
PROJECT_DIR="$(cd "$PROJECT_DIR" && pwd)"
[ -f "$PROJECT_DIR/CMakeLists.txt" ] || die "$PROJECT_DIR has no CMakeLists.txt"
[ -f "$PROJECT_DIR/config.h" ] || die "$PROJECT_DIR has no config.h"
OUT="${OUT:-$PROJECT_DIR/build-linux/dist}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
[ ${#VARIANTS[@]} -gt 0 ] || VARIANTS=("")

host_arch() { [ "$(uname -s)" = Linux ] && uname -m || echo none; }
[ ${#ARCHS[@]} -gt 0 ] || { [ "$(host_arch)" = none ] && ARCHS=(aarch64 x86_64) || ARCHS=("$(host_arch)"); }
for a in "${ARCHS[@]}"; do
  case "$a" in aarch64|x86_64) ;; *) die "--arch takes aarch64 or x86_64, not $a" ;; esac
done

VERSION="$(sed -n 's/^#define PLUG_VERSION_STR "\(.*\)"/\1/p' "$PROJECT_DIR/config.h")"
[ -n "$VERSION" ] || die "could not read PLUG_VERSION_STR from $PROJECT_DIR/config.h"

# ------------------------------------------------------------------------------------------------ Docker
run_in_docker() {  # arch
  local arch="$1" platform image var
  case "$arch" in
    aarch64) platform=linux/arm64; image=iplug2-linux:22.04-gcc13 ;;
    x86_64)  platform=linux/amd64; image=iplug2-linux:22.04-gcc13-amd64 ;;
  esac
  var="IPLUG_LINUX_IMAGE_$arch"
  image="${!var:-$image}"
  docker image inspect "$image" >/dev/null 2>&1 \
    || die "no Docker image $image for $arch: build it as linux-research/docker/README.md says, or set $var"

  local mounts=() dir common
  for dir in "$IPLUG2_DIR" "$PROJECT_DIR" "$OUT"; do
    mounts+=(-v "$dir:$dir")
    # Linked worktrees keep their git data in the main checkout; the build info and dirty check need it.
    common="$(git -C "$dir" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
    case "$common" in ""|"$dir"/*) ;; *) mounts+=(-v "$common:$common:ro") ;; esac
  done

  local args=("$PROJECT_DIR" --formats "$FORMATS" --glibc-max "$GLIBC_MAX") v
  for v in "${VARIANTS[@]}"; do [ -z "$v" ] || args+=(--variant "$v"); done
  for v in "${DOCS[@]+"${DOCS[@]}"}"; do args+=(--doc "$v"); done
  [ -z "$ICON" ] || args+=(--icon "$ICON")
  [ $KEEP_GOING = 0 ] || args+=(--keep-going)
  [ $CLEAN = 0 ] || args+=(--clean)

  echo "=== $arch: running in $image ($platform)"
  docker run --rm --platform "$platform" "${mounts[@]}" -w "$PROJECT_DIR" \
    -e SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-}" \
    "$image" bash "$SELF" "${args[@]}" --arch "$arch" --out "$OUT" --in-container
}

if [ "$IN_CONTAINER" = 0 ]; then
  native="$(host_arch)"
  local_archs=()
  for a in "${ARCHS[@]}"; do
    if [ "$a" = "$native" ]; then local_archs+=("$a"); else run_in_docker "$a"; fi
  done
  [ ${#local_archs[@]} -gt 0 ] || exit 0
fi

# -------------------------------------------------------------------------------------------- Linux build
[ "$(uname -s)" = Linux ] || die "internal: build step on $(uname -s)"
ARCH="$(uname -m)"

if [ -z "${CC:-}" ] && command -v gcc-13 >/dev/null; then export CC=gcc-13 CXX=g++-13; fi
for tool in cmake ninja objcopy strip readelf tar gzip; do
  command -v $tool >/dev/null || die "$tool not found"
done

GIT_COMMIT="$(git -C "$PROJECT_DIR" rev-parse --short=7 HEAD 2>/dev/null || echo unknown)"
DIRTY=""
if [ -n "$(git -C "$PROJECT_DIR" status --porcelain --untracked-files=no 2>/dev/null)" ]; then
  DIRTY="-dirty"
  echo "WARNING: $PROJECT_DIR has uncommitted changes; the archives are marked $GIT_COMMIT$DIRTY" >&2
fi
# Archive timestamps follow the commit, so the same commit packs the same tar headers.
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git -C "$PROJECT_DIR" log -1 --format=%ct 2>/dev/null || echo 0)}"

glibc_floor() { readelf -W --dyn-syms "$1" | grep -oE '@GLIBC_[0-9.]+' | sed 's/@GLIBC_//' | sort -Vu | tail -1; }
version_gt() { [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -1)" = "$1" ] && [ "$1" != "$2" ]; }
needed() { readelf -d "$1" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'; }
exports() { readelf -W --dyn-syms "$1" | awk '$7 != "UND" && $5 != "LOCAL" && $4 != "NOTYPE" {print $8}' | sed 's/@.*//' | sort -u; }

# Fails the package rather than shipping a binary a customer's distro cannot load, or one that leaks
# symbols into the host. The standalone's NEEDED list is reported, not limited: it links the audio/MIDI
# and SWELL libraries by design.
check_binary() {  # file kind report
  local f="$1" kind="$2" report="$3" ok=0 floor bad
  floor="$(glibc_floor "$f")"
  {
    echo "### $(basename "$f") ($kind)"
    echo "build-id: $(readelf -n "$f" | sed -n 's/.*Build ID: //p')"
    echo "GLIBC floor: $floor"
    echo "NEEDED: $(needed "$f" | tr '\n' ' ')"
    if [ "$kind" = app ]; then
      echo "exports: $(exports "$f" | wc -l) symbols"
    else
      echo "exports: $(exports "$f" | tr '\n' ' ')"
    fi
  } >> "$report"
  if version_gt "$floor" "$GLIBC_MAX"; then
    echo "$f needs GLIBC_$floor > $GLIBC_MAX: build on Ubuntu 22.04 (or in Docker from another host), or raise --glibc-max" >&2; ok=1
  fi
  if readelf -W --dyn-syms "$f" | grep -qE '@(GLIBCXX|CXXABI)_'; then
    echo "$f imports libstdc++ symbols; it must link libstdc++ statically" >&2; ok=1
  fi
  case "$kind" in
    clap) bad="$(exports "$f" | grep -vx clap_entry || true)" ;;
    vst3) bad="$(exports "$f" | grep -vxE 'GetPluginFactory|ModuleEntry|ModuleExit' || true)" ;;
    app)  bad="" ;;
  esac
  [ -z "$bad" ] || { echo "$f exports more than its entry points: $bad" >&2; ok=1; }
  if [ "$kind" != app ]; then
    bad="$(needed "$f" | grep -vxE 'libX11\.so\.6|libc\.so\.6|libm\.so\.6|ld-linux.*\.so\.[0-9]+' || true)"
    [ -z "$bad" ] || { echo "$f needs libraries a plug-in must not: $bad" >&2; ok=1; }
  fi
  return $ok
}

# The .build-id/ links let gdb find the symbols with `set debug-file-directory <unpacked dir>`.
split_symbols() {  # binary debug-dir
  local f="$1" dbg="$2/$(basename "$1").debug" id
  objcopy --only-keep-debug --compress-debug-sections=zlib "$f" "$dbg"
  strip --strip-unneeded "$f"
  objcopy --add-gnu-debuglink="$dbg" "$f"
  id="$(readelf -n "$f" | sed -n 's/.*Build ID: //p')"
  [ -n "$id" ] || die "$f has no build id"
  mkdir -p "$2/.build-id/${id:0:2}"
  ln -sf "../../$(basename "$dbg")" "$2/.build-id/${id:0:2}/${id:2}.debug"
}

# Modes from the build tree depend on the builder's umask; --system installs need world-readable files.
normalize_modes() {  # dir
  find "$1" -type d -exec chmod 755 {} +
  find "$1" -type f -exec chmod 644 {} +
}

deterministic_tar() {  # archive dir-parent dir-name
  tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$SOURCE_DATE_EPOCH" \
      -C "$2" -cf - "$3" | gzip -n -9 > "$1"
}

package_variant() {  # variant-spec
  local spec="$1" vname="${1%%:*}" vargs=""
  [[ "$spec" == *:* ]] && vargs="${spec#*:}"
  local suffix="${vname:+_$vname}"
  local build="$PROJECT_DIR/build-linux/package/$ARCH$suffix"
  echo "=== $ARCH${vname:+ $vname}: configure ($build)"
  [ $CLEAN = 0 ] || rm -rf "$build"
  mkdir -p "$build"

  # -g does not change GCC's generated code; it only feeds the split debug-symbol archive.
  # shellcheck disable=SC2086
  cmake -S "$PROJECT_DIR" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DIPLUG2_DIR="$IPLUG2_DIR" -DIPLUG_DEPLOY_PLUGINS=OFF -DIPLUG2_DISABLE_DEPRECATION_WARNINGS=ON \
        -DCMAKE_C_FLAGS=-g -DCMAKE_CXX_FLAGS=-g $vargs > "$build/package-configure.log" 2>&1 \
    || { tail -30 "$build/package-configure.log" >&2; die "configure failed: $build/package-configure.log"; }

  local name targets=() fmt
  name="$(sed -n 's/^CMAKE_PROJECT_NAME:STATIC=//p' "$build/CMakeCache.txt")"
  for fmt in $FORMATS; do
    case "$fmt" in clap|vst3|app) targets+=("$name-$fmt") ;; *) die "--formats: unknown format $fmt" ;; esac
  done
  echo "=== $ARCH${vname:+ $vname}: build ${targets[*]}"
  cmake --build "$build" --target "${targets[@]}" > "$build/package-build.log" 2>&1 \
    || { tail -40 "$build/package-build.log" >&2; die "build failed: $build/package-build.log"; }

  local pkg="${name}_v${VERSION}_linux_${ARCH}${suffix}"
  local stage="$PROJECT_DIR/build-linux/package/stage/$pkg"
  local dstage="$PROJECT_DIR/build-linux/package/stage/${pkg}_debug-symbols"
  rm -rf "$stage" "$dstage"
  mkdir -p "$stage" "$dstage"
  local report="$OUT/$pkg.inspect.txt"
  echo "$pkg — $name $VERSION, $GIT_COMMIT$DIRTY, built $(date -u +%F) on $(. /etc/os-release && echo "$PRETTY_NAME"), ${CXX:-c++} $("${CXX:-c++}" -dumpfullversion)" > "$report"

  local out="$build/out" failed=0
  for fmt in $FORMATS; do
    case "$fmt" in
      clap)
        mkdir -p "$stage/CLAP"
        cp -a "$out/$name.clap" "$stage/CLAP/"
        [ -d "$out/$name.resources" ] && cp -a "$out/$name.resources" "$stage/CLAP/"
        split_symbols "$stage/CLAP/$name.clap" "$dstage"
        check_binary "$stage/CLAP/$name.clap" clap "$report" || failed=1 ;;
      vst3)
        mkdir -p "$stage/VST3"
        cp -a "$out/$name.vst3" "$stage/VST3/"
        local so="$stage/VST3/$name.vst3/Contents/$ARCH-linux/$name.so"
        [ -f "$so" ] || die "VST3 binary missing: $so"
        split_symbols "$so" "$dstage"
        check_binary "$so" vst3 "$report" || failed=1 ;;
      app)
        mkdir -p "$stage/Standalone"
        cp -a "$out/$name" "$stage/Standalone/"
        [ -d "$out/$name.resources" ] && cp -a "$out/$name.resources" "$stage/Standalone/"
        split_symbols "$stage/Standalone/$name" "$dstage"
        check_binary "$stage/Standalone/$name" app "$report" || failed=1 ;;
    esac
  done
  local doc
  for doc in "${DOCS[@]+"${DOCS[@]}"}"; do cp "$PROJECT_DIR/$doc" "$stage/"; done
  if [ -n "$ICON" ] && has_format app; then
    cp "$PROJECT_DIR/$ICON" "$stage/Standalone/$name.${ICON##*.}"
  fi
  write_install_sh "$stage/install.sh" "$name" "$([ -n "$ICON" ] && echo "$name.${ICON##*.}")"
  write_readme "$stage/README-Linux.txt" "$name" "$vname" "$report"
  [ $failed = 0 ] || { [ $KEEP_GOING = 1 ] || die "binary checks failed for $pkg (see above, and $report)"; }

  normalize_modes "$stage"
  normalize_modes "$dstage"
  chmod 755 "$stage/install.sh"
  [ ! -d "$stage/Standalone" ] || chmod 755 "$stage/Standalone/$name"

  deterministic_tar "$OUT/$pkg.tar.gz" "$(dirname "$stage")" "$pkg"
  deterministic_tar "$OUT/${pkg}_debug-symbols.tar.gz" "$(dirname "$dstage")" "${pkg}_debug-symbols"
  (cd "$(dirname "$stage")" && find "$pkg" | sort) > "$OUT/$pkg.files.txt"
  echo "=== $OUT/$pkg.tar.gz ($(($(stat -c %s "$OUT/$pkg.tar.gz") / 1024)) KiB), symbols: ${pkg}_debug-symbols.tar.gz"
}

write_install_sh() {  # path name icon
  # The CLAP and VST3 specs' install locations; the standalone goes beside them as a self-contained folder.
  cat > "$1" <<EOF
#!/bin/sh
# Installs $2 for the current user, or for every user with --system (run it with sudo).
#   ./install.sh [--system] [--uninstall] [--no-clap] [--no-vst3] [--no-app]
set -eu
NAME="$2"
ICON="$3"
EOF
  cat >> "$1" <<'EOF'
HERE="$(cd "$(dirname "$0")" && pwd)"
SYSTEM=0; UNINSTALL=0; CLAP=1; VST3=1; APP=1
for arg in "$@"; do
  case "$arg" in
    --system) SYSTEM=1 ;;
    --uninstall) UNINSTALL=1 ;;
    --no-clap) CLAP=0 ;;
    --no-vst3) VST3=0 ;;
    --no-app) APP=0 ;;
    -h|--help) sed -n '2,3p' "$0" | sed 's/^# //'; exit 0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done
[ -d "$HERE/CLAP" ] || CLAP=0
[ -d "$HERE/VST3" ] || VST3=0
[ -d "$HERE/Standalone" ] || APP=0

# The standalone gets a folder of its own: ~/.local/share/<Name> is where the product keeps the user's
# data (presets), which an install or uninstall must never touch.
if [ $SYSTEM = 1 ]; then
  [ "$(id -u)" = 0 ] || { echo "--system needs root: sudo ./install.sh --system" >&2; exit 1; }
  CLAP_DIR=/usr/lib/clap; VST3_DIR=/usr/lib/vst3
  APP_DIR=/opt/$NAME; BIN_DIR=/usr/local/bin; DESKTOP_DIR=/usr/share/applications
else
  [ "$(id -u)" != 0 ] || { echo "run without sudo for a per-user install, or pass --system" >&2; exit 1; }
  CLAP_DIR=$HOME/.clap; VST3_DIR=$HOME/.vst3
  APP_DIR=$HOME/.local/lib/$NAME; BIN_DIR=$HOME/.local/bin
  DESKTOP_DIR=${XDG_DATA_HOME:-$HOME/.local/share}/applications
fi
DESKTOP_FILE="$DESKTOP_DIR/$(echo "$NAME" | tr 'A-Z ' 'a-z-').desktop"
APP_MARKER=".installed-by-$NAME-install.sh"

# Only a folder this script created is removed whole.
remove_app_dir() {
  [ -e "$APP_DIR" ] || return 0
  [ -f "$APP_DIR/$APP_MARKER" ] || { echo "$APP_DIR was not created by this installer; leaving it alone" >&2; exit 1; }
  rm -rf "$APP_DIR"
}

if [ $UNINSTALL = 1 ]; then
  [ $CLAP = 0 ] || rm -rf "$CLAP_DIR/$NAME.clap" "$CLAP_DIR/$NAME.resources"
  [ $VST3 = 0 ] || rm -rf "$VST3_DIR/$NAME.vst3"
  if [ $APP = 1 ]; then remove_app_dir; rm -f "$BIN_DIR/$NAME" "$DESKTOP_FILE"; fi
  echo "$NAME removed."
  exit 0
fi

# Replace, never merge: a resource file dropped by a newer version must not linger from an older one.
replace() {  # src dest
  rm -rf "$2"
  mkdir -p "$(dirname "$2")"
  cp -R "$1" "$2"
}
if [ $CLAP = 1 ]; then
  replace "$HERE/CLAP/$NAME.clap" "$CLAP_DIR/$NAME.clap"
  if [ -d "$HERE/CLAP/$NAME.resources" ]; then
    replace "$HERE/CLAP/$NAME.resources" "$CLAP_DIR/$NAME.resources"
  fi
  echo "CLAP -> $CLAP_DIR/$NAME.clap"
fi
if [ $VST3 = 1 ]; then
  replace "$HERE/VST3/$NAME.vst3" "$VST3_DIR/$NAME.vst3"
  echo "VST3 -> $VST3_DIR/$NAME.vst3"
fi
if [ $APP = 1 ]; then
  remove_app_dir
  replace "$HERE/Standalone" "$APP_DIR"
  : > "$APP_DIR/$APP_MARKER"
  mkdir -p "$BIN_DIR" "$DESKTOP_DIR"
  ln -sf "$APP_DIR/$NAME" "$BIN_DIR/$NAME"
  {
    echo "[Desktop Entry]"
    echo "Type=Application"
    echo "Name=$NAME"
    echo "Exec=\"$APP_DIR/$NAME\""
    [ -z "$ICON" ] || echo "Icon=$APP_DIR/$ICON"
    echo "Categories=AudioVideo;Audio;"
    echo "Terminal=false"
  } > "$DESKTOP_FILE"
  echo "Standalone -> $APP_DIR/$NAME (link $BIN_DIR/$NAME, menu entry $DESKTOP_FILE)"
fi
echo "Done. Rescan plug-ins in your host if it does not pick $NAME up."
EOF
  chmod 755 "$1"
}

has_format() { [[ " $FORMATS " == *" $1 "* ]]; }

write_readme() {  # path name variant report
  local name="$2" title floor
  title="$name $VERSION${3:+ ($3)} for Linux ($ARCH)"
  floor="$(sed -n 's/^GLIBC floor: //p' "$4" | sort -Vu | tail -1)"
  {
    echo "$title"
    printf '%s\n\n' "$(printf '%s' "$title" | tr '[:print:]' '=')"
    echo "Contents"
    has_format clap && echo "  CLAP/        $name.clap and $name.resources/ (the plug-in needs both, side by side)"
    has_format vst3 && echo "  VST3/        $name.vst3 (a folder; its resources are inside)"
    has_format app && echo "  Standalone/  $name, a standalone application, and $name.resources/"
    echo "  install.sh   copies them to the standard locations"
    cat <<EOF

Install
  ./install.sh                 for your user:   ~/.clap, ~/.vst3, ~/.local/lib/$name
  sudo ./install.sh --system   for all users:   /usr/lib/clap, /usr/lib/vst3, /opt/$name
  Options: --no-clap, --no-vst3, --no-app, --uninstall. Then rescan plug-ins in your host.

  To install by hand, copy $name.clap together with $name.resources/ into ~/.clap (or
  /usr/lib/clap), and $name.vst3 into ~/.vst3 (or /usr/lib/vst3). A .clap without its
  .resources folder next to it loads without its fonts and images.

Requirements
  - A 64-bit $ARCH Linux with glibc $floor or newer (Ubuntu 22.04, Debian 12,
    Fedora 36 and later).
  - An X11 session (Wayland desktops run it through XWayland).
  - libX11, and an OpenGL 2.1 driver (libGL.so.1, from Mesa or your GPU vendor).
  - Optional: libfontconfig (fonts in menus), libXcursor (themed cursors), and zenity,
    kdialog or xdg-desktop-portal for file dialogs.
EOF
    has_format app && cat <<EOF
  - The standalone application also needs GTK 3, ALSA, PulseAudio and JACK client
    libraries: libgtk-3-0, libasound2, libpulse0 and libjack-jackd2-0 (or PipeWire's
    JACK libraries) on Debian/Ubuntu; gtk3, alsa-lib, pulseaudio-libs and
    pipewire-jack-audio-connection-kit-libs on Fedora. Most desktops have them.
EOF
    printf '\nBuild: %s\n' "$GIT_COMMIT$DIRTY"
  } > "$1"
}

for spec in "${VARIANTS[@]}"; do
  package_variant "$spec"
done
