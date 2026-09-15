#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
output=${1:?absolute empty output directory is required}
python3 "$root/.github/rpg-runtime/candidate_descriptor.py" prepare "$output"
mkdir -p "$root/.retrom-work"
work=$(mktemp -d "$root/.retrom-work/web.XXXXXX")
mkdir -p "$work/raw" "$work/build"
source_digest=$(python3 "$root/.github/rpg-runtime/candidate_descriptor.py" digest "$output")
python3 "$root/.github/rpg-runtime/candidate_descriptor.py" archive "$work/source.tar"
export RETROM_HOST_UID="$(id -u)" RETROM_HOST_GID="$(id -g)"
if ! docker run --rm --platform linux/amd64 --hostname retrom-gam4980 \
  --env RETROM_HOST_UID --env RETROM_HOST_GID \
  --volume "$work/source.tar:/source.tar:ro" \
  --volume "$root/.github/rpg-runtime:/recipe:ro" \
  --volume "$work/build:/work" --volume "$work/raw:/output" \
  emscripten/emsdk@sha256:af45409f3199d88db4b1b03af0098532c8fb33a375ac257463eeb0a622870d06 \
  /recipe/build-emulatorjs-core.sh >"$work/build.log" 2>&1; then
  tail -100 "$work/build.log" >&2
  exit 1
fi
test "$source_digest" = "$(python3 "$root/.github/rpg-runtime/candidate_descriptor.py" digest "$output")"
stage="$work/stage"
mkdir -p "$stage"
install -m 0644 "$work/raw/gam4980_libretro.js" "$stage/"
install -m 0644 "$work/raw/gam4980_libretro.wasm" "$stage/"
cat "$root/COPYING" "$work/raw/retroarch-COPYING" > "$stage/license.txt"
printf '%s\n' '{"minimumEJSVersion":"4.2.2","version":"0.2-retrom1"}' > "$stage/build.json"
printf '%s\n' '{"name":"gam4980","extensions":["gam"],"options":{},"save":true,"license":"COPYING","repo":"https://github.com/retrom-project/gam4980"}' > "$stage/core.json"
(cd "$stage" && 7z a -mtm=off -mta=off -mtc=off -bd -bso0 -bsp0 -t7z "$output/gam4980-wasm.data" \
  gam4980_libretro.js gam4980_libretro.wasm build.json core.json license.txt)
install -m 0644 "$stage/license.txt" "$output/LICENSE"
gzip -n -c "$work/source.tar" > "$output/source.tar.gz"
