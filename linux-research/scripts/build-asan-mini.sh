#!/usr/bin/env bash
# Debug + ASan IPlugControls CLAP plus the reviewer's mini CLAP host (evidence/p3a-review/mini-clap-host.cpp).
# Runs INSIDE the container (dock.sh), cwd linux-research. Usage: build-asan-mini.sh [builddir=build/p3a-asan]
set -uo pipefail
B="${1:-build/p3a-asan}"
A="-fsanitize=address -fno-omit-frame-pointer"
cmake -S .. -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DIPLUG_DEPLOY_PLUGINS=OFF -DIPLUG2_DISABLE_DEPRECATION_WARNINGS=ON \
  "-DCMAKE_CXX_FLAGS=$A" "-DCMAKE_C_FLAGS=$A" "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address" \
  "-DCMAKE_MODULE_LINKER_FLAGS=-fsanitize=address" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address" || exit 2
cmake --build "$B" --target IPlugControls-clap || exit 1
g++ -std=c++17 -O1 -g $A -I../Dependencies/IPlug/CLAP_SDK/include evidence/p3a-review/mini-clap-host.cpp \
  -o "$B/mini-clap-host" -ldl -lX11 || exit 1
echo "ASAN BUILD OK"
