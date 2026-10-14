#!/usr/bin/env bash
# T-007: editors on the VM's GNOME/X11 desktop at a desktop scale. Sets the screen mode and GNOME's scaling, runs the
# scale probe (host calls and sizes), the REAPER step tests with scaled steps and the IPlugEffect APP, and puts every
# setting back afterwards, also on failure.
# Usage (host side): vm-run.sh vm/probe/scale-desktop.sh <out> <mode> <scaling-factor> <text-scaling-factor> \
#                      <REAPER's scale> <the APP's scale> [what...]
#   <mode>: an xrandr mode, or - to keep the screen as it is. UTM's display agent sets the mode back to the window's
#   size (1440x863 here) at its own pace, so - is the reliable choice.
#   e.g. - 2 1.0 2 2 for GNOME at 200 %; - 0 1.5 1 1.5 for large text (Xft.dpi 144, UI scale 1)
#   what: probe-clap probe-vst3 reaper-clap reaper-vst3 app (default: all); xact steps are scaled by the scale given
set -uo pipefail
OUT="$1"; MODE="$2"; SF="$3"; TSF="$4"; REAPER_SCALE="$5"; APP_SCALE="$6"; shift 6
WHAT="${*:-probe-clap probe-vst3 reaper-clap reaper-vst3 app}"
HERE="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$OUT"
IFACE=org.gnome.desktop.interface
OUTPUT=$(xrandr | awk '/ connected/{print $1; exit}')
OLD_MODE=$(xrandr | awk '/\*/{print $1; exit}')
OLD_SF=$(gsettings get $IFACE scaling-factor | awk '{print $NF}')
OLD_TSF=$(gsettings get $IFACE text-scaling-factor)
restore() {
  gsettings set $IFACE scaling-factor "$OLD_SF"; gsettings set $IFACE text-scaling-factor "$OLD_TSF"
  [ "$MODE" = - ] || xrandr --output "$OUTPUT" --mode "$OLD_MODE"; sleep 2
  echo "restored: $(xrandr | awk '/\*/{print $1; exit}'), scaling-factor $(gsettings get $IFACE scaling-factor), text $(gsettings get $IFACE text-scaling-factor)"
}
trap restore EXIT
[ "$MODE" = - ] || xrandr --output "$OUTPUT" --mode "$MODE"
gsettings set $IFACE scaling-factor "$SF"
gsettings set $IFACE text-scaling-factor "$TSF"
sleep 5
{ xrandr | head -2; echo "scaling-factor $(gsettings get $IFACE scaling-factor), text-scaling-factor $(gsettings get $IFACE text-scaling-factor)"
  xprop -root RESOURCE_MANAGER; } | tee "$OUT/desktop.txt"
for w in $WHAT; do
  case "$w" in
    probe-clap) "$HERE/scale-probe.sh" "$OUT/probe-clap" "CLAP: IPlugEffect" "$HOME/.clap/IPlugEffect.clap" keep > "$OUT/probe-clap.log" 2>&1;;
    probe-vst3) "$HERE/scale-probe.sh" "$OUT/probe-vst3" "VST3: IPlugEffect" "$HOME/.vst3/IPlugEffect.vst3" keep > "$OUT/probe-vst3.log" 2>&1;;
    reaper-clap) XACT_SCALE="$REAPER_SCALE" "$HERE/reaper-plugin-test.sh" "$OUT/reaper-effect-clap" "CLAP: IPlugEffect" \
                   "$HOME/.clap/IPlugEffect.clap" scripts/steps/iplugeffect-p3a.txt > "$OUT/reaper-effect-clap.log" 2>&1;;
    reaper-vst3) XACT_SCALE="$REAPER_SCALE" "$HERE/reaper-plugin-test.sh" "$OUT/reaper-effect-vst3" "VST3: IPlugEffect" \
                   "$HOME/.vst3/IPlugEffect.vst3" scripts/steps/iplugeffect-p3a.txt > "$OUT/reaper-effect-vst3.log" 2>&1;;
    app) XACT_SCALE="$APP_SCALE" APP_ARGS=--no-io "$HERE/vm/probe/app-test.sh" "$OUT/app-effect" "$HOME/iplug-apps/IPlugEffect" \
           scripts/steps/iplugeffect-p3a.txt > "$OUT/app-effect.log" 2>&1;;
  esac
  pkill -x reaper 2>/dev/null; sleep 1
  case "$w" in
    probe-*) echo "$w: $(tr '\n' ';' < "$OUT/$w/calls.txt")";;
    reaper-*) echo "$w: $(head -1 "$OUT/reaper-effect-${w#reaper-}/RESULT.txt")";;
    app) echo "app: $(grep -c FAIL "$OUT/app-effect/xact.log") step failures; $(head -1 "$OUT/app-effect/RESULT.txt")";;
  esac
done
