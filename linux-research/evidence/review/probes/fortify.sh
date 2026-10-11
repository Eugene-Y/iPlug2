#!/usr/bin/env bash
cd /Volumes/Storage/code/IPlug2/linux-research/build/review-race
for o in "-O2" "-O2 -D_FORTIFY_SOURCE=2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" "-O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3" "-O0"; do
  g++ $o fortify.cpp -o fortify && echo "[$o] -> $(./fortify 2>&1 | tr '\n' ' ') exit=$?"
done
g++ -O2 -dM -E -x c++ /dev/null | grep FORTIFY || echo "no FORTIFY predefined"
