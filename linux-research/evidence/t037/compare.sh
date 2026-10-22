#!/bin/bash
# compare.sh <worktree> <before-ref> <after-ref> <outdir>
# Duplicates every example at six placements with duplicate.py from <before-ref> and <after-ref>, once natively (mac)
# and once with dup-emul.py's Windows separator emulation (win), plus the other-drive case (win only).
# Each side gets its own copy of Examples/ and Scripts/ in <outdir>/<mode>/<side>/iPlug2 with the same layout around
# it, so the written relative paths compare byte for byte.
set -u
WT=$1; BEFORE=$2; AFTER=$3; OUT=$4
EV=$(cd "$(dirname "$0")" && pwd)
rm -rf "$OUT"; mkdir -p "$OUT"
EXAMPLES=$(cd "$WT/Examples" && for d in */; do echo "${d%/}"; done)

for mode in mac win; do
  for side in before after; do
    ref=$BEFORE; [ $side = after ] && ref=$AFTER
    R="$OUT/$mode/$side"; L="$OUT/logs/$mode/$side"
    mkdir -p "$R/iPlug2" "$R/sib" "$R/deep/a/b/c" "$R/with space/x" "$L"
    git -C "$WT" archive "$ref" Examples Scripts | tar -x -C "$R/iPlug2"
    for ex in $EXAMPLES; do
      short=${ex#IPlug}
      for place in Intree Dot Parent Sib Deep Space; do
        case $place in
          Intree) args=();;
          Dot)    args=(.);;
          Parent) args=(../..);;
          Sib)    args=(../../sib);;
          Deep)   args=("$R/deep/a/b/c");;
          Space)  args=("$R/with space/x");;
        esac
        log="$L/$ex-$place.log"
        (cd "$R/iPlug2/Examples" && python3 "$EV/dup-emul.py" $mode "$ex" "X$short$place" Mfr ${args[@]+"${args[@]}"}) > "$log" 2>&1
        echo "exit=$?" >> "$log"
      done
    done
    if [ $mode = win ]; then
      mkdir -p "$R/driveD/out"
      (cd "$R/iPlug2/Examples" && SIM_OTHER_DRIVE="$R/driveD" python3 "$EV/dup-emul.py" win IPlugEffect XEffectDrive Mfr "$R/driveD/out") > "$L/drive.log" 2>&1
      echo "exit=$?" >> "$L/drive.log"
    fi
  done
done
