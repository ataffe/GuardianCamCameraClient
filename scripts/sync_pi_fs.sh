#!/usr/bin/env bash
set -euo pipefail

mkdir -p ../pi-sysroot

rsync -avzR --copy-unsafe-links \
  --exclude='/usr/lib/jvm' \
  --exclude='/usr/lib/aspell' \
  --exclude='/usr/lib/firmware' \
  --exclude='/usr/lib/cups' \
  --exclude='/usr/lib/modules' \
  --exclude='/usr/lib/ssl/private' \
  --exclude='/usr/lib/systemd' \
  alex@10.0.0.144:/lib \
  alex@10.0.0.144:/usr/include \
  alex@10.0.0.144:/usr/lib \
  alex@10.0.0.144:/usr/local \
  ../pi-sysroot/