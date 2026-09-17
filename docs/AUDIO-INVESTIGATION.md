# Melody audio implementation and evidence

The core implements the two on-chip melody channels and the melody timer. Actual
firmware programs MTCT/ML1D/ML2D, receives the MT interrupt and advances the game's
own score. No replacement soundtrack or game-specific note sequence is embedded.
Output is signed 16-bit stereo at 44.1 kHz (the mono hardware mix is duplicated),
735 samples per 60 Hz frame. Both libretro batch and single-sample callbacks work;
a stalled callback cannot stall emulation or grow an audio queue.

## Register and hardware evidence

- [BA4988 system reconstruction](https://gitee.com/BA4988/ba4988-system),
  `os/registers.h`, `os/interrupt.cpp` and `os/dictsys.cpp`, identifies MTCT at
  $022B, ML1D/ML2D at $022C/$022D, PWMVOL at $023E and AUDCON at $023F. Its
  melody routines program these registers and service TISR/TIER bit 5, vector
  $0344. The OS volume routine provides fourteen ordered PWM settings.
- [BBK A100 simulator register definitions](https://gitee.com/BA4988/BBK-A100-simulator-V2),
  `src/a100.h`, independently names ML1EN/ML2EN as AUDCON bits 6/7. It separates
  the on-chip melody registers from the speech channels.
- The original teardown, *步步高朗文4980电子词典原理详解*, 电子报 2010-04-04,
  page 317 ([scan, PDF page 5](https://d1.amobbs.com/bbs_upload782111/files_30/ourdev_562983M6R75B.pdf)),
  identifies an MS37020 MCU and a separate 53C691D speech DAC and amplifier.
  [BBK patent CN2596419Y](https://patents.google.com/patent/CN2596419Y/zh)
  also identifies the MS37020 and 4 MHz CPU crystal.

## Reconstructed behavior and limits

Tone codes are reconstructed as an eight-bit LFSR counter,
`next = (x << 1) | parity(x & 0x8e)`, counting through terminal code 1. Zero
is a rest. Multiple note pairs observed in the operator's sound-enabled game
have exact octave periods under this encoding: 67/AD = 124/62, 1B/D4 = 110/55,
8B/04 = 98/49 and 86/C9 = 74/37. These relationships distinguish the encoding
from a linear register-to-frequency mapping. Tests use synthetic register
programs, not those game or firmware bytes.

The implementation uses a 65,536 Hz tone edge clock and 128 Hz melody timer,
independent of the CPU/ST rate options. Those clocks are reconstructed from the
RTC clock domain and firmware reload patterns; they have **not** been calibrated
against an MS37020 datasheet or a recording from physical hardware. The square
wave is integrated over each output sample to reduce aliasing. The OS's ordered
PWM settings use monotonic gains; original analogue waveform and amplifier
response are not claimed. This is functional melody emulation, not a claim of
cycle-accurate pitch, tempo or timbre reproduction.

The timer advances while a melody channel is enabled, even with its interrupt
masked. Overflow reloads MTCT and latches TISR bit 5; an enabled pending interrupt
wakes HALT and reaches the real CPU vector. CPU deadlines at each audio sample
bound register/interrupt scheduling without a deferred event queue.

The external dictionary speech-ROM/DAC handshake remains disconnected; dictionary
pronunciation is not supported. Music also requires a game containing music data:
the checked segments of the operator's `伏魔记-1.0` sample did not exercise the
same music sequence as `伏魔记(有声版)`.

## State and verification

`BBKST002` stores both tone phases, the melody timer and its fractional phase.
Fresh instances resume with identical PCM output. The bounded `BBKST001` reader
preserves the previous machine state and initializes audio phases to zero; old
states had no emulated audio phase to restore. Reset/deinit clear audio state.

Run `.github/rpg-runtime/test-native.sh`. Audio regressions cover silence, both
callbacks, partial/stalled batches, independent channels, octave relationships,
volume, bounded mixing, real IRQ dispatch, masking/HALT, clock-rate independence,
reset, fresh-instance PCM replay and both state versions with corruption rejection.
Retrom's BBKRPG product acceptance additionally verifies actual browser output,
volume/mute/pause, gamepad input and a different Launch restoring the checkpoint.
Operator game, BIOS, recordings and temporary research files stay out of Git and
all source/release archives.
