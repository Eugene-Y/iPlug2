# iplug2-linux:22.04 plus GCC 13 from the Ubuntu toolchain PPA: the glibc 2.35 floor with C++20 <format>,
# which GCC 11 (22.04's default) lacks and Sand uses. iPlug2 links libstdc++ statically, so the newer
# compiler adds no GLIBCXX requirement.
# Build: docker build -t iplug2-linux:22.04-gcc13 linux-research/docker/gcc13
# Use:   IMAGE=iplug2-linux:22.04-gcc13 DOCK_ENV="CC=gcc-13 CXX=g++-13" scripts/dock.sh -- ...
FROM iplug2-linux:22.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends software-properties-common gpg-agent \
 && add-apt-repository -y ppa:ubuntu-toolchain-r/test \
 && apt-get update && apt-get install -y --no-install-recommends gcc-13 g++-13 \
 && rm -rf /var/lib/apt/lists/*
