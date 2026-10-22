#!/usr/bin/env bash
# mac proof for T-018: IGraphicsSkia.cpp/.h preprocess identically on macOS before and after (old + <mutex> on top),
# and the new file passes -fsyntax-only, for Metal, CPU and GL3.
set -u
W=/Volumes/Storage/code/IPlug2-T-018; SP="$1"; BASE="$2"; cd "$W"
mkdir -p "$SP/old-h"
git show $BASE:IGraphics/Drawing/IGraphicsSkia.cpp > "$SP/old.cpp"
sed 's/^#include <map>$/#include <map>\
#include <mutex>/' "$SP/old.cpp" > "$SP/old-mutex.cpp"
git show $BASE:IGraphics/Drawing/IGraphicsSkia.h > "$SP/old-h/IGraphicsSkia.h"
cp IGraphics/Drawing/IGraphicsSkia.cpp "$SP/new.cpp"
S=Dependencies/Build/src/skia
INC=(-I IGraphics -I IGraphics/Controls -I IGraphics/Platforms -I IGraphics/Extras -I IPlug -I WDL
     -I Dependencies/IGraphics/NanoSVG/src -I Dependencies/IGraphics/STB -I $S -I $S/include -I $S/modules/svg/include)
for cfg in IGRAPHICS_METAL IGRAPHICS_CPU IGRAPHICS_GL3; do
  for v in old-mutex new; do
    if [ $v = new ]; then H=(-I IGraphics/Drawing); else H=(-I "$SP/old-h" -I IGraphics/Drawing); fi
    clang++ -std=c++17 -x objective-c++ -E -P -DIGRAPHICS_SKIA -D$cfg "${H[@]}" "${INC[@]}" "$SP/$v.cpp" -o "$SP/$v-$cfg.i" 2> "$SP/$v-$cfg.err"
    echo "$cfg $v preprocess exit=$? lines=$(wc -l < "$SP/$v-$cfg.i")"
  done
  cmp "$SP/old-mutex-$cfg.i" "$SP/new-$cfg.i" && echo "$cfg: preprocessed output IDENTICAL"
  clang++ -std=c++17 -x objective-c++ -fsyntax-only -DIGRAPHICS_SKIA -D$cfg -I IGraphics/Drawing "${INC[@]}" "$SP/new.cpp" 2> "$SP/syntax-$cfg.err"
  echo "$cfg new -fsyntax-only exit=$? errors=$(grep -c 'error:' "$SP/syntax-$cfg.err")"
done
