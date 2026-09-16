# Audio implementation status

Audio is not implemented in this candidate. Keep the runtime volume capability
false until a real native PCM path is verified. No fabricated soundtrack or
uncalibrated oscillator has been added to the core.

Confirmed from the maintained source:

- The single-sample callback is stored but never called; the batch setter is empty.
- Writes to PB are forced to zero, and the source explicitly calls out disabled audio.
- The interrupt dispatcher handles MT (TISR/TIER bit 5, vector $0344), but the
  peripheral clock never advances an MT counter or raises that interrupt.
- The current ST1-4/CT model is insufficient to implement the remaining sound hardware.

An isolated, unshipped local probe raised MT interrupts while using the operator's
BIOS and sound-enabled game sample. The firmware then programmed $022B-$022D and
AUDCON ($023F) repeatedly and eventually disabled the music interrupt. This confirms
that interrupt generation is a missing part of the native playback path. It does
not establish the MT clock divider, tone-frequency encoding, output-enable bits,
waveform, mixer, or speech/DAC behavior. A fixed-rate interrupt alone is not an
acceptable sound implementation.

The checked segments of the operator's `伏魔记-1.0` sample did not exercise the same
music sequence as `伏魔记(有声版)`. Neither sample nor any BIOS bytes belong in tests,
source archives, or published evidence. Future audio regressions should use
project-owned synthetic instructions and cover audible samples versus silence,
callback fallback, pause/reset, rate independence, and deterministic fresh-instance
state continuation. Any added peripheral state needs an explicit state version and
backward reading of existing complete `BBKST001` states.
