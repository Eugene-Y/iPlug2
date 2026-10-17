# A customer's machine for release smoke tests (T-021): a distro with only runtime packages — no -dev
# packages, no compiler — plus the test tools (Xvfb, openbox, xdotool, ImageMagick, gdb, validators, REAPER).
# Build: docker build --build-arg BASE=ubuntu:22.04 -t iplug2-rt:ubuntu22.04 linux-research/docker/runtime
#        BASE = ubuntu:22.04 | ubuntu:24.04 | debian:12 | fedora:latest; x86_64: --platform linux/amd64, tag -amd64
# Use:   IMAGE=iplug2-rt:ubuntu22.04 linux-research/scripts/package-smoke.sh <archive.tar.gz> <outdir>
ARG BASE=ubuntu:22.04

# Validators built on 22.04 (glibc 2.35, static libstdc++) run on every target distro; the main image's
# are built on 24.04 and need glibc 2.38.
FROM ubuntu:22.04 AS tools
ARG DEBIAN_FRONTEND=noninteractive
ARG VST3SDK_TAG=v3.8.1_build_84
ARG CLAP_VALIDATOR_REF=0.4.1
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl git build-essential ninja-build pkg-config python3 python3-pip \
      libx11-dev libx11-xcb-dev libxcb1-dev libxcb-util-dev libxcb-cursor-dev libxcb-keysyms1-dev \
      libxcb-xkb-dev libxkbcommon-dev libxkbcommon-x11-dev libcairo2-dev libpango1.0-dev \
      libfontconfig1-dev libfreetype-dev libgtkmm-3.0-dev libsqlite3-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --no-cache-dir 'cmake>=3.25,<4'
RUN curl -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
RUN git clone https://github.com/free-audio/clap-validator.git /src/clap-validator \
 && cd /src/clap-validator && git checkout ${CLAP_VALIDATOR_REF} \
 && /root/.cargo/bin/cargo build --release \
 && install -m755 target/release/clap-validator /usr/local/bin/clap-validator
RUN git clone --branch ${VST3SDK_TAG} --depth 1 --recursive --shallow-submodules \
      https://github.com/steinbergmedia/vst3sdk.git /src/vst3sdk
RUN cmake -S /src/vst3sdk -B /src/vst3build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=ON \
      -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_RUN_VST_VALIDATOR=OFF \
      -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
 && cmake --build /src/vst3build --target validator \
 && find /src/vst3build -type f -name validator -perm -u+x -exec install -m755 {} /usr/local/bin/ \;

FROM ${BASE}
ARG REAPER_URL_BASE=https://www.reaper.fm/files/7.x
ARG REAPER_VER=782
ARG DEBIAN_FRONTEND=noninteractive
# Plug-in runtime (README-Linux.txt): libX11, libGL + Mesa, fontconfig, Xcursor. The standalone's: GTK 3,
# ALSA, PulseAudio, JACK. REAPER: GTK 3. Then the test tools.
# Ubuntu 24.04 renamed libasound2 and libgtk-3-0 (64-bit time_t: ...t64).
RUN if command -v apt-get >/dev/null; then \
      apt-get update \
      && t64=$(apt-cache show libasound2t64 >/dev/null 2>&1 && echo t64 || true) \
      && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils procps file binutils \
        libx11-6 libgl1 libgl1-mesa-dri libfontconfig1 libxcursor1 \
        libgtk-3-0$t64 libasound2$t64 libpulse0 libjack-jackd2-0 \
        xvfb openbox dbus-x11 x11-utils x11-xserver-utils xdotool imagemagick gdb lua5.4 xclip xsel \
      && rm -rf /var/lib/apt/lists/*; \
    else \
      dnf install -y --setopt=install_weak_deps=False \
        ca-certificates curl xz procps-ng file binutils \
        libX11 mesa-libGL mesa-dri-drivers fontconfig libXcursor \
        gtk3 alsa-lib pulseaudio-libs pipewire-jack-audio-connection-kit-libs \
        xorg-x11-server-Xvfb openbox dbus-x11 xwininfo xdpyinfo xsetroot xdotool ImageMagick gdb lua \
        xclip xsel dejavu-sans-fonts \
      && dnf clean all; \
    fi
RUN for c in cc gcc c++ g++ clang; do \
      if command -v $c >/dev/null; then echo "$c present: not a runtime-only image" >&2; exit 1; fi; \
    done; \
    dev=$( (dpkg-query -W -f '${Package}\n' 2>/dev/null; rpm -qa --qf '%{NAME}\n' 2>/dev/null) | grep -E -- '-(dev|devel)$' || true); \
    if [ -n "$dev" ]; then echo "development packages present: $dev" >&2; exit 1; fi
COPY --from=tools /usr/local/bin/clap-validator /usr/local/bin/validator /usr/local/bin/
RUN arch=$(uname -m) \
 && curl -fsSL ${REAPER_URL_BASE}/reaper${REAPER_VER}_linux_${arch}.tar.xz | tar -xJ -C /opt \
 && mv /opt/reaper_linux_${arch}/REAPER /opt/REAPER && rm -rf /opt/reaper_linux_${arch}
RUN useradd -m customer
ENV LIBGL_ALWAYS_SOFTWARE=1 \
    GALLIUM_DRIVER=llvmpipe \
    XDG_RUNTIME_DIR=/tmp/xdg \
    NO_AT_BRIDGE=1
RUN mkdir -p /tmp/xdg && chmod 700 /tmp/xdg
WORKDIR /work
