FROM debian:trixie

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git rsync openssh-client \
    pkg-config symlinks file \
    ca-certificates \
    crossbuild-essential-arm64 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work