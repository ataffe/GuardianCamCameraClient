#!/usr/bin/env bash
set -euo pipefail

# Run the container as the invoking user so edge-agent-build/ ends up owned by
# them rather than by root (otherwise the next `rm -rf` from the host fails).
# An arbitrary uid has no home directory in the image, so point HOME at somewhere
# writable: CMake's FetchContent and the git clones it drives both write there.
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -v "$PWD":/work \
  -v "$PWD"/../pi-sysroot:/sysroot:ro \
  magpie-cross \
  bash -c '
    cmake -B edge-agent-build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/work/toolchain-pi.cmake \
      -DCMAKE_BUILD_TYPE=Release &&
    cmake --build edge-agent-build -j$(nproc)
  '
