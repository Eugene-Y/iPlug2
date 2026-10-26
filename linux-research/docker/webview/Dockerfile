# Adds WebKitGTK (the out-of-process WebView helper's build dependency, T-017) to an existing image without
# rebuilding it: the main Dockerfile has the same layer for fresh builds.
#   docker build --build-arg BASE=iplug2-linux:24.04 -t iplug2-linux:24.04 linux-research/docker/webview
ARG BASE=iplug2-linux:24.04
FROM ${BASE}
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends libwebkit2gtk-4.1-dev \
    && rm -rf /var/lib/apt/lists/*
