# Base image for a reproducible Paper Pro build shell.
#
# The official reMarkable SDK installer is deliberately not embedded here: it is
# firmware-specific, proprietary, and should be supplied by the developer who
# owns the tablet. Mount or install the matching SDK, source its environment
# script, and then run /workspace/build-rmpp.sh.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    ninja-build \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
CMD ["bash"]
