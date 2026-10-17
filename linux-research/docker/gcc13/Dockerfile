# iplug2-linux:22.04 plus GCC 13 from the Ubuntu toolchain PPA (why: docker/README.md).
# Build: docker build -t iplug2-linux:22.04-gcc13 linux-research/docker/gcc13
#        x86_64: --platform linux/amd64 --build-arg BASE=iplug2-linux:22.04-amd64 -t iplug2-linux:22.04-gcc13-amd64
# Use:   IMAGE=iplug2-linux:22.04-gcc13 DOCK_ENV="CC=gcc-13 CXX=g++-13" scripts/dock.sh -- ...
ARG BASE=iplug2-linux:22.04
FROM ${BASE}
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends software-properties-common gpg-agent \
 && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
 && apt-get update && apt-get install -y --no-install-recommends gcc-13 g++-13 \
 && rm -rf /var/lib/apt/lists/*
