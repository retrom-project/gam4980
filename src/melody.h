/* Native two-channel melody peripheral. See docs/AUDIO-INVESTIGATION.md for
   the register evidence, reconstructed counter encoding and fidelity limits. */
#define MELODY_SAMPLE_RATE 44100u
#define MELODY_FRAME_SAMPLES (MELODY_SAMPLE_RATE / 60u)
#define MELODY_TICK_RATE 128u
#define MELODY_TONE_CLOCK 65536u
#define MELODY_MAX_PHASE (2u * 255u * MELODY_SAMPLE_RATE)

static struct {
    uint32_t phase[2];
    uint32_t timer_phase;
    uint16_t timer;
} melody;
/* Derived lookup and the current output frame are not emulated state. */
static uint16_t melody_period[256];
static int16_t melody_pcm[MELODY_FRAME_SAMPLES * 2];

static void melody_reset(void)
{
    memset(&melody, 0, sizeof(melody));
    memset(melody_pcm, 0, sizeof(melody_pcm));
    melody_period[0] = 0; /* zero is a rest, the locked LFSR state */
    unsigned code = 1;
    for (unsigned count = 1; count <= 255; ++count) {
        melody_period[code] = count;
        /* Inverse of (x << 1) | parity(x & 0x8e). */
        unsigned top = (code ^ (code >> 2) ^ (code >> 3) ^ (code >> 4)) & 1;
        code = (code >> 1) | (top << 7);
    }
}

static void melody_write(uint16_t addr, uint8_t value, uint8_t previous)
{
    if (addr == _MTCT)
        melody.timer = value;
    else if (addr == _ML1D || addr == _ML2D)
        melody.phase[addr - _ML1D] = 0;
    else if (addr == _AUDCON) {
        for (unsigned channel = 0; channel < 2; ++channel)
            if ((value ^ previous) & (0x40u << channel))
                melody.phase[channel] = 0;
        if (!(value & 0xc0))
            melody.timer_phase = 0;
    }
}

static void melody_timer_tick(void)
{
    if (sys.ram[_AUDCON] & 0xc0) {
        melody.timer_phase += MELODY_TICK_RATE;
        if (melody.timer_phase >= MELODY_SAMPLE_RATE) {
            melody.timer_phase -= MELODY_SAMPLE_RATE;
            if (++melody.timer == 256) {
                melody.timer = sys.ram[_MTCT];
                sys.ram[_TISR] |= 0x20;
            }
        }
    }
    if (sys.ram[_TISR] & sys.ram[_TIER] & 0x20)
        sys.ram[_SYSCON] &= 0xf7;
}

static int32_t melody_integral(uint32_t phase, uint32_t half_period)
{
    return (int32_t)(phase <= half_period ? phase : 2 * half_period - phase);
}

static int16_t melody_sample(void)
{
    melody_timer_tick();
    int32_t mixed = 0;
    for (unsigned channel = 0; channel < 2; ++channel) {
        uint16_t period = melody_period[sys.ram[_ML1D + channel]];
        if (!(sys.ram[_AUDCON] & (0x40u << channel)) || !period)
            continue;
        uint32_t half = period * MELODY_SAMPLE_RATE;
        uint32_t before = melody.phase[channel] % (2 * half);
        uint32_t after = (before + MELODY_TONE_CLOCK) % (2 * half);
        /* Integrate the square wave over each sample. This preserves sub-sample
           edges and reduces aliasing without a host-dependent floating phase. */
        int32_t area = melody_integral(after, half) - melody_integral(before, half);
        mixed += (int64_t)area * 8192 / MELODY_TONE_CLOCK;
        melody.phase[channel] = after;
    }
    /* The OS exposes fourteen PWM settings in this order. Use monotonic gains;
       the analogue amplifier's response is deliberately not claimed here. */
    static const uint8_t volume_levels[16] = {14,8,13,7,12,6,11,5,10,4,9,3,2,2,1,1};
    return (int16_t)(mixed * volume_levels[sys.ram[_PWMVOL] >> 4] / 14);
}

static void melody_submit(void)
{
    size_t offset = 0;
    while (audio_batch_cb && offset < MELODY_FRAME_SAMPLES) {
        size_t remaining = MELODY_FRAME_SAMPLES - offset;
        size_t accepted = audio_batch_cb(melody_pcm + offset * 2, remaining);
        if (!accepted || accepted > remaining)
            break;
        offset += accepted;
    }
    if (audio_cb)
        for (; offset < MELODY_FRAME_SAMPLES; ++offset)
            audio_cb(melody_pcm[offset * 2], melody_pcm[offset * 2 + 1]);
    /* A frontend accepting neither callback loses this frame, never emulation
       time or unbounded memory. The next run always produces a fresh frame. */
}
