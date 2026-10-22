#!/bin/bash
# analyse.sh <outdir of compare.sh>: the checks behind compare-summary.txt.
set -u
O=$1
cd "$O"
FWD='-name *.xcconfig -o -name project.pbxproj -o -name *.mk -o -name makedist-*.sh -o -name prepare_resources-*.py'
VS='-name *.props -o -name *.vcxproj -o -name *.vcxproj.filters'
norm() { sed -E "s#$O/[a-z]+/(before|after)#<R>#g"; }

echo "# duplicate.py before/after: 20 examples x 6 placements (Intree = 3 args, Dot = '.', Parent = '../..',"
echo "# Sib = '../../sib', Deep = <R>/deep/a/b/c, Space = '<R>/with space/x'), random.seed(11)."
echo "# mac = native run; win = dup-emul.py's Windows separator emulation. <R> = the side's root, iPlug2 at <R>/iPlug2."
echo
echo "## exit codes (per mode/side: 120 runs; the win-only other-drive run is at the end)"
for m in mac win; do for s in before after; do
  echo "$m/$s: $(grep -h '^exit=' logs/$m/$s/*-*.log | sort | uniq -c | tr -s ' ' | tr '\n' ' ')"
done; done
echo
echo "## mac: before vs after, every output file (only duplicate.py itself may differ)"
diff -rq mac/before mac/after | grep -v 'iPlug2/Examples/duplicate.py ' || echo "identical"
echo "## mac: before vs after, console output (paths normalised)"
n=0; for f in logs/mac/before/*.log; do
  cmp -s <(norm < "$f") <(norm < "logs/mac/after/${f##*/}") || { echo "differs: ${f##*/}"; n=$((n+1)); }
done; [ $n = 0 ] && echo "identical ($(ls logs/mac/before | wc -l | tr -d ' ') logs)"
echo
echo "## win after vs mac after, every output file (the emulated Windows output should equal macOS's)"
diff -rq mac/after win/after | grep -v 'Only in win/after: driveD' || echo "identical"
echo
echo "## win: before vs after, files that differ, by kind (VS files must not appear)"
diff -rq win/before win/after | grep -v -e driveD -e 'Examples/duplicate.py ' | sed -E 's|^Files win/before/||; s| and .*||' \
  | sed -E 's|.*/||; s|^X[A-Za-z0-9]+(Intree\|Dot\|Parent\|Sib\|Deep\|Space)-|<X>-|' | sort | uniq -c
echo
echo "## mac/iOS/web files holding a '\\' path (count of files, out-of-tree duplicates)"
for t in mac/after win/before win/after; do
  echo "$t: $(find $t \( $FWD \) -path '*X*' -print0 | xargs -0 grep -l '\.\.\\' | wc -l | tr -d ' ')"
done
echo "## any file with a doubled root ('..\\iPlug2\\iPlug2' or '../iPlug2/iPlug2')"
for t in mac/after win/before win/after; do
  echo "$t: $(grep -rlE '\.\.[\\/]iPlug2[\\/]iPlug2' $t | wc -l | tr -d ' ')"
done
echo
echo "## VS files: '/' paths written into them (expect 0 everywhere)"
for t in mac/after win/before win/after; do
  echo "$t: $(find $t \( $VS \) -path '*X*' -print0 | xargs -0 grep -lE '\.\./\.\.|iPlug2/' | wc -l | tr -d ' ')"
done
echo
echo "## IPlugEffect: root lines per placement"
for p in Dot Parent Sib Deep Space; do
  for t in win/before win/after mac/after; do
    d=$(find $t -maxdepth 6 -type d -name "XEffect$p" | head -1)
    echo "$p $t:"
    grep -hE '^ *(IPLUG2_ROOT ?=|<IPLUG2_ROOT>)' "$d/config/XEffect$p-mac.xcconfig" "$d/config/XEffect$p-web.mk" "$d/scripts/makedist-web.sh" \
      "$d/scripts/prepare_resources-win.py" "$d/config/XEffect$p-win.props" | sed 's/^/    /'
  done
done
echo
echo "## win/after: every out-of-tree duplicate's roots resolve to <R>/iPlug2"
bad=0; ok=0
while IFS= read -r x; do
  name=${x##*/}; root=$(cd win/after/iPlug2 && pwd -P)
  r=$(sed -n 's/^IPLUG2_ROOT = //p' "$x/config/$name-mac.xcconfig")
  [ "$(cd "$x/config" && cd "$r" 2>/dev/null && pwd -P)" = "$root" ] && ok=$((ok+1)) || { bad=$((bad+1)); echo "xcconfig: $x"; }
  props=$(ls "$x/config/"*.props 2>/dev/null | head -1)
  if [ -n "$props" ]; then
    r=$(sed -n 's|.*<IPLUG2_ROOT>$(ProjectDir)\(.*\)</IPLUG2_ROOT>.*|\1|p' "$props" | tr '\\' /)
    [ "$(cd "$x/projects" && cd "$r" 2>/dev/null && pwd -P)" = "$root" ] && ok=$((ok+1)) || { bad=$((bad+1)); echo "props: $x"; }
  fi
done < <(find win/after -type d -name 'X*' -prune | grep -v Intree)
echo "resolved: $ok, wrong: $bad"
echo
echo "## other drive (win, SIM_OTHER_DRIVE): target <R>/driveD/out"
for s in before after; do
  echo "$s: $(grep '^exit=' logs/win/$s/drive.log); last error line: $(grep -E '^(error|ValueError)' logs/win/$s/drive.log | norm)"
  echo "    left in the target: $(ls win/$s/driveD/out | tr '\n' ' ')"
  [ -d win/$s/driveD/out/XEffectDrive ] && echo "    XEffectDrive/config: $(ls win/$s/driveD/out/XEffectDrive/config | head -3 | tr '\n' ' ')..."
done
