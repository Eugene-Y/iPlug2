#!/bin/sh
# Collects facts about this computer that help us reproduce a problem with Gneiss or Sand on Linux, into one
# text file. It only reads: it installs and changes nothing, uses no network and sends nothing. It records no
# user or computer name; your home folder is written as ~. Read the file before you send it.
#   sh collect-linux-report.sh [output-file]
OUT="${1:-linux-report-$(date +%Y%m%d-%H%M).txt}"
ARCH="$(uname -m)"
have() { command -v "$1" >/dev/null 2>&1; }
section() { printf '\n== %s\n' "$1"; }
show() { [ -n "$2" ] && printf '%-22s %s\n' "$1:" "$2"; }
build_id() {
  if have readelf; then readelf -n "$1" 2>/dev/null | sed -n 's/.*Build ID: //p'
  else file -L "$1" 2>/dev/null | sed -n 's/.*BuildID\[[a-z0-9]*\]=\([0-9a-f]*\).*/\1/p'; fi
}
case "$ARCH" in x86_64) LIBTAG='libc6,x86-64' ;; aarch64) LIBTAG='libc6,AArch64' ;; *) LIBTAG='libc6' ;; esac
lib() { ldconfig -p 2>/dev/null | grep -qF "$1 ($LIBTAG" && echo found || echo MISSING; }
[ -x /sbin/ldconfig ] && PATH="$PATH:/sbin"

report() {
  echo "Linux report, $(date -u '+%Y-%m-%d %H:%M UTC')"
  section System
  show Distribution "$(. /etc/os-release 2>/dev/null && echo "$PRETTY_NAME")"
  show Kernel "$(uname -srm)"
  show glibc "$(getconf GNU_LIBC_VERSION 2>/dev/null)"
  show CPU "$(lscpu 2>/dev/null | sed -n 's/^Model name:[[:space:]]*//p' | head -1)"
  [ "$ARCH" = x86_64 ] && show AVX2 "$(grep -qw avx2 /proc/cpuinfo && echo yes || echo no)"
  show Memory "$(awk '/^MemTotal/ {printf "%.1f GB", $2 / 1048576}' /proc/meminfo)"

  section Desktop
  show Session "${XDG_SESSION_TYPE:-unknown} (DISPLAY ${DISPLAY:-unset}, WAYLAND_DISPLAY ${WAYLAND_DISPLAY:-unset})"
  show Desktop "${XDG_CURRENT_DESKTOP:-unknown}"
  show GDK_SCALE "${GDK_SCALE:-unset}"
  show QT_SCALE_FACTOR "${QT_SCALE_FACTOR:-unset}"
  have xrdb && show Xft.dpi "$(xrdb -query 2>/dev/null | sed -n 's/^Xft.dpi:[[:space:]]*//p')"
  have gsettings && show "GNOME scaling" "$(gsettings get org.gnome.desktop.interface scaling-factor 2>/dev/null)"
  have gsettings && show "GNOME text scaling" "$(gsettings get org.gnome.desktop.interface text-scaling-factor 2>/dev/null)"
  have xrandr && xrandr --current 2>/dev/null | grep -E ' connected|\*'

  section Graphics
  have lspci && lspci -nnk 2>/dev/null | grep -A3 -E 'VGA|3D controller|Display controller' | grep -vE 'Subsystem|modules'
  if have glxinfo; then glxinfo -B 2>&1
  else echo "glxinfo is not installed (Ubuntu/Debian: sudo apt install mesa-utils; Fedora: sudo dnf install glx-utils)"; fi

  section Audio
  have pactl && pactl info 2>/dev/null | grep -E '^(Server Name|Server Version|Default Sample Specification):'

  section Libraries
  for l in libX11.so.6 libGL.so.1 libXext.so.6 libfontconfig.so.1 libXcursor.so.1 libdbus-1.so.3 \
           libgtk-3.so.0 libasound.so.2 libpulse.so.0 libjack.so.0; do show "$l" "$(lib "$l")"; done
  for p in zenity kdialog; do show "$p" "$(have "$p" && echo found || echo missing)"; done
  show xdg-desktop-portal "$(pgrep -f xdg-desktop-portal >/dev/null 2>&1 && echo running || echo 'not running')"

  section "Installed Gneiss and Sand"
  for n in Gneiss Sand; do
    for f in "$HOME/.clap/$n.clap" /usr/lib/clap/$n.clap \
             "$HOME/.vst3/$n.vst3/Contents/$ARCH-linux/$n.so" /usr/lib/vst3/$n.vst3/Contents/$ARCH-linux/$n.so \
             "$HOME/.local/lib/$n/$n" /opt/$n/$n; do
      [ -f "$f" ] || continue
      id="$(build_id "$f")"
      echo "$f  build-id ${id:-unknown (needs readelf or file)}"
      case "$f" in
        *.clap) [ -d "${f%.clap}.resources" ] || echo "  $n.resources is missing next to it" ;;
        */$n) ldd "$f" 2>/dev/null | grep 'not found' ;;
      esac
    done
  done

  section "Recent crashes"
  if have coredumpctl; then coredumpctl list --no-pager --since=-7d 2>/dev/null | tail -n 10
  else echo "coredumpctl is not installed"; fi
  ls /var/crash 2>/dev/null | sed 's|_home_[^_]*_|_home_<user>_|'
}

if [ -n "$HOME" ] && [ "$HOME" != / ]; then report 2>&1 | sed "s|$HOME|~|g" > "$OUT"
else report > "$OUT" 2>&1; fi
[ -s "$OUT" ] || { echo "Could not write $OUT" >&2; exit 1; }
echo "Written to $OUT. Please add your host (DAW) name and version, and what you did when the problem happened."
