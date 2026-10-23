#!/usr/bin/env bash
set -e
D=/Volumes/Storage/code/IPlug2-T-023/linux-research/build/t023r-presettest
cmake -S $D/IPlugEffect -B $D/b -G Ninja -DCMAKE_BUILD_TYPE=Debug -DIPLUG2_DIR=/Volumes/Storage/code/IPlug2-T-023 -DIPLUG_DEPLOY_PLUGINS=OFF -DIPLUG2_DISABLE_DEPRECATION_WARNINGS=ON > $D/cmake.log
cmake --build $D/b --target IPlugEffect-clap | tail -2
g++ -std=c++17 -g -I/Volumes/Storage/code/IPlug2-T-023/Dependencies/IPlug/CLAP_SDK/include $D/host.cpp -o $D/host -ldl -lpthread
echo BUILT; find $D/b -name "*.clap" -maxdepth 3
