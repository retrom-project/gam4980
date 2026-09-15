# GAM4980 browser core maintenance

## Sources and ownership

The upstream mirror is ThisBoringWorld/gam4980 at
`eeaa531b55e7127ab4b5e0bdc5ceba686df59c6a`. The maintained baseline is
`retrom/geeaa531b55e7`; `main` remains an upstream mirror.
`retrom-fork.json` is the machine-readable source and artifact contract.

The browser build links the libretro core to the pinned EmulatorJS RetroArch
commit recorded in that file, using the immutable Emscripten SDK image in the
build recipe. It produces a single-threaded EmulatorJS 4.2.3 core.
Retrom identifies the platform as `bbkrpg` and the core/Target as `gam4980`.

## Input and firmware

The core accepts original single-file `.gam` packages with a valid GAM header,
an in-range entry point and data bank offset, bounded to 1920 KiB.
It requires exactly 2 MiB each of `gam4980/8.BIN` and `gam4980/E.BIN` in the
frontend system directory. Firmware is user-supplied and excluded from artifacts.
The OS initialization loop is bounded and invalid firmware fails loading.

Libretro A maps only to ENTER, B only to EXIT, and the D-pad to directions.
Other native dictionary keys retain their upstream joypad mappings. Keyboard
input remains independent. The upstream implementation disables the audio
hardware; this integration does not claim sound emulation.

## Instant states

The native `BBKST001` state is a fixed-size, little-endian snapshot with complete
RAM, Flash, CPU registers, memory banks, timer/RTC remainders, input repeat state,
LCD persistence/pixels and core options. It contains no process pointers or BIOS.
A checksum rejects corruption before mutation, and fingerprints bind it to the
loaded game and BIOS. Restoring reconstructs memory mappings, including Flash
identification mode. Upstream partial snapshots are intentionally unsupported.
The Provider gives this core its own versioned checkpoint format and applies its
shared transport compression exactly once.

## Verification and candidate builds

Run `.github/rpg-runtime/test-native.sh`. Tests use project-owned synthetic
instructions, never inherited commercial games or the operator's firmware.
They cover malformed input, bounds, corruption without state mutation, Flash,
keyboard bounds, actual CPU execution, fresh-instance deterministic continuation,
cross-game rejection and short firmware rejection.

Use Retrom `pfb-core-build CORE=gam4980` for candidate bytes. Its entry point is
`.github/rpg-runtime/build-candidate.sh <absolute-empty-output-directory>`.
The source archive is deterministic and explicitly excludes upstream ROM/BIOS
examples, local evidence and build outputs. Candidate metadata records the
actual branch, commit, dirty state and source digest; it is not a release pin.

A release requires the matching Retrom product acceptance case before tagging.
Publish immutable core artifacts first, then pin the verified runtime Provider,
then update Retrom's production lock and repeat the same product checks.
