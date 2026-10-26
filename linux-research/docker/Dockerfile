# iPlug2 Linux build & test environment.
# Native arch (arm64 on Apple silicon); also builds for linux/amd64.
# Build:  docker build -t iplug2-linux:24.04 linux-research/docker
# Old-glibc variant: --build-arg UBUNTU=22.04 -t iplug2-linux:22.04
#   (build-only: the validators are always built on 24.04 — vst3sdk 3.8 needs CMake >= 3.25 — so
#    they do not run in the 22.04 image; test with the 24.04 image.)
ARG UBUNTU=24.04
ARG TOOLS_UBUNTU=24.04

# ---------------------------------------------------------------- validators
FROM ubuntu:${TOOLS_UBUNTU} AS tools
ARG DEBIAN_FRONTEND=noninteractive
ARG VST3SDK_TAG=v3.8.1_build_84
ARG CLAP_VALIDATOR_REF=0.4.1
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl git build-essential cmake ninja-build pkg-config python3 \
      libx11-dev libx11-xcb-dev libxcb1-dev libxcb-util-dev libxcb-cursor-dev libxcb-keysyms1-dev \
      libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libcairo2-dev libpango1.0-dev \
      libfontconfig1-dev libfreetype-dev libgtkmm-3.0-dev libsqlite3-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# clap-validator (Rust)
RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
RUN git clone https://github.com/free-audio/clap-validator.git /src/clap-validator \
 && cd /src/clap-validator && git checkout ${CLAP_VALIDATOR_REF} \
 && /root/.cargo/bin/cargo build --release \
 && install -m755 target/release/clap-validator /usr/local/bin/clap-validator

# VST3 SDK validator + editorhost
RUN git clone --branch ${VST3SDK_TAG} --depth 1 --recursive --shallow-submodules \
      https://github.com/steinbergmedia/vst3sdk.git /src/vst3sdk
RUN cmake -S /src/vst3sdk -B /src/vst3build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=ON \
      -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_RUN_VST_VALIDATOR=OFF \
 && cmake --build /src/vst3build --target validator editorhost \
 && find /src/vst3build -type f \( -name validator -o -name editorhost \) -perm -u+x \
      -exec install -m755 {} /usr/local/bin/ \;

# ---------------------------------------------------------------- runtime/dev
FROM ubuntu:${UBUNTU}
ARG DEBIAN_FRONTEND=noninteractive
ARG REAPER_URL_BASE=https://www.reaper.fm/files/7.x
ARG REAPER_VER=782
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl wget git xz-utils unzip file sudo less \
      build-essential clang lld cmake ninja-build pkg-config python3 python3-pip \
      gdb strace ltrace binutils patchelf \
      libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev \
      libxkbcommon-dev libxkbcommon-x11-dev libx11-xcb-dev libxcb1-dev libxfixes-dev libxrender-dev \
      libgl1-mesa-dev libegl1-mesa-dev libglu1-mesa-dev libgles2-mesa-dev mesa-utils libgl1-mesa-dri \
      libfontconfig1-dev libfreetype-dev libasound2-dev libjack-jackd2-dev libpulse-dev \
      libgtk-3-dev libcurl4-openssl-dev \
      libxcb-util-dev libxcb-icccm4-dev libxcb-xfixes0-dev libxcb-keysyms1-dev libxcb-cursor-dev \
      jackd2 \
      xvfb x11-utils x11-apps xdotool imagemagick openbox dbus-x11 zenity xauth \
      fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*

COPY --from=tools /usr/local/bin/clap-validator /usr/local/bin/validator /usr/local/bin/editorhost /usr/local/bin/

# REAPER (evaluation; portable install under /opt/REAPER)
RUN arch=$(uname -m) \
 && curl -fsSL ${REAPER_URL_BASE}/reaper${REAPER_VER}_linux_${arch}.tar.xz | tar -xJ -C /opt \
 && mv /opt/reaper_linux_${arch}/REAPER /opt/REAPER && rm -rf /opt/reaper_linux_${arch}

ENV LIBGL_ALWAYS_SOFTWARE=1 \
    GALLIUM_DRIVER=llvmpipe \
    XDG_RUNTIME_DIR=/tmp/xdg \
    NO_AT_BRIDGE=1
RUN mkdir -p /tmp/xdg && chmod 700 /tmp/xdg

# P3b: clipboard round-trip checks (a separate layer, so the layers above stay cached)
RUN apt-get update && apt-get install -y --no-install-recommends xclip xsel \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work

# T-017: WebKitGTK for the out-of-process WebView helper (own last layer, so the layers above stay cached)
RUN apt-get update && apt-get install -y --no-install-recommends libwebkit2gtk-4.1-dev \
    && rm -rf /var/lib/apt/lists/*
