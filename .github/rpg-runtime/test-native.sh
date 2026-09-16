#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
mkdir -p "$root/.retrom-work"
cc=${CC:-cc}
# Fixed addresses avoid PIE/ASan shadow collisions with older SDK compilers on WSL.
# Bound each test so a broken emulator or sanitizer cannot stall a candidate build.
for source in core_regression core_lifecycle; do
  "$cc" -std=gnu11 -O1 -g -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie -no-pie \
    "$root/tests/$source.c" -o "$root/.retrom-work/$source"
done
for case_name in malformed-game state-bounds flash-roundtrip corrupt-state keyboard-bounds timer-remainder timer-cadence cpu-slice; do
  timeout --kill-after=2s 30s "$root/.retrom-work/core_regression" "$case_name"
done
for case_name in invalid-inputs cross-instance incompatible-content broken-bios; do
  timeout --kill-after=2s 30s "$root/.retrom-work/core_lifecycle" "$case_name"
done
