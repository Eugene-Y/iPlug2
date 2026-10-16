#!/usr/bin/env bash
# Provision the UTM test VM (Ubuntu 24.04 arm64, GNOME on X11) from the Mac. Idempotent: re-running only fills gaps.
#   - apt packages for running and debugging plug-ins on the real desktop
#   - REAPER $REAPER_VER for the VM's arch in ~/opt/REAPER, with /opt/REAPER -> ~/opt/REAPER so the Docker
#     harness scripts (which hard-code /opt/REAPER) run unchanged
#   - ~/.config/REAPER/reaper.ini seeded with the keys the Docker harness uses (no splash, Dummy Audio)
#   - ~/.clap, ~/.vst3, ~/iplug-apps, and the harness scripts mirrored to ~/$VM_HARNESS/linux-research/scripts
# Usage: vm-provision.sh            (env: VM_HOST, VM_KEY, REAPER_VER=782)
set -euo pipefail
. "$(dirname "$0")/vm-env.sh"
REAPER_VER="${REAPER_VER:-782}"
REAPER_URL_BASE="${REAPER_URL_BASE:-https://www.reaper.fm/files/7.x}"

PKGS="gdb strace x11-utils xdotool xclip xsel wmctrl mesa-utils imagemagick scrot rsync sysstat
      jackd2 build-essential libx11-dev libxfixes-dev"

vm "REAPER_VER=$REAPER_VER REAPER_URL_BASE=$REAPER_URL_BASE PKGS='$(echo $PKGS)' bash -s" <<'REMOTE'
set -euo pipefail
missing=""
for p in $PKGS; do
  [ "$(dpkg-query -W -f='${db:Status-Status}' "$p" 2>/dev/null)" = installed ] || missing="$missing $p"
done
if [ -n "${missing// }" ]; then
  echo "apt: installing $missing"
  # jackd2 asks whether to enable realtime limits; the dummy backend needs none.
  echo "jackd2 jackd/tweak_rt_limits boolean false" | sudo -n debconf-set-selections
  sudo -n DEBIAN_FRONTEND=noninteractive apt-get update -qq
  sudo -n DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends $missing
else
  echo "apt: all packages present"
fi

arch=$(uname -m)
stamp="$HOME/opt/REAPER/.installed-version"
if [ "$(cat "$stamp" 2>/dev/null)" = "$REAPER_VER $arch" ] && [ -x "$HOME/opt/REAPER/reaper" ]; then
  echo "REAPER $REAPER_VER $arch already in ~/opt/REAPER"
else
  tmp=$(mktemp -d)
  echo "REAPER: downloading reaper${REAPER_VER}_linux_${arch}.tar.xz"
  curl -fsSL "$REAPER_URL_BASE/reaper${REAPER_VER}_linux_${arch}.tar.xz" | tar -xJ -C "$tmp"
  mkdir -p "$HOME/opt"; rm -rf "$HOME/opt/REAPER"
  mv "$tmp/reaper_linux_${arch}/REAPER" "$HOME/opt/REAPER"; rm -rf "$tmp"
  echo "$REAPER_VER $arch" > "$stamp"
  echo "REAPER installed: $(ls -l "$HOME/opt/REAPER/reaper")"
fi
if [ "$(readlink /opt/REAPER 2>/dev/null)" != "$HOME/opt/REAPER" ]; then
  [ -e /opt/REAPER ] && [ ! -L /opt/REAPER ] && { echo "/opt/REAPER exists and is not our symlink; leaving it"; exit 1; }
  sudo -n ln -sfn "$HOME/opt/REAPER" /opt/REAPER; echo "/opt/REAPER -> ~/opt/REAPER"
fi

# Same keys as reaper-up.sh writes into its throwaway profile. Existing values are kept.
ini="$HOME/.config/REAPER/reaper.ini"
mkdir -p "$(dirname "$ini")"
[ -f "$ini" ] || printf '[REAPER]\n' > "$ini"
for kv in splashscreen=0 linux_audio_mode=2; do
  grep -q "^${kv%%=*}=" "$ini" || sed -i "/^\[REAPER\]/a $kv" "$ini"
done
echo "reaper.ini: $(grep -E '^(splashscreen|linux_audio_mode)=' "$ini" | tr '\n' ' ')"

mkdir -p "$HOME/.clap" "$HOME/.vst3" "$HOME/iplug-apps"
echo "ptrace_scope=$(cat /proc/sys/kernel/yama/ptrace_scope) (left as is; the harness runs gdb through sudo)"
REMOTE

vm_sync_harness
echo "harness scripts mirrored to $VM_HOST:~/$VM_HARNESS/linux-research/scripts"
