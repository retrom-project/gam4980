/* Project-owned register programs; no game, firmware or soundtrack fixtures. */
#include <assert.h>
#include <stdio.h>
#include "../src/libretro.c"
#include "core_fixture.h"

#define SAMPLE_RATE 44100
#define FRAME_SAMPLES 735
static int16_t captured[SAMPLE_RATE * 2];
static size_t captured_frames, batch_calls, single_calls, batch_limit = SIZE_MAX;

static void collect(int16_t left, int16_t right)
{
    assert(left == right);
    assert(captured_frames < SAMPLE_RATE);
    captured[2 * captured_frames] = left;
    captured[2 * captured_frames++ + 1] = right;
}

static size_t collect_batch(const int16_t *data, size_t frames)
{
    ++batch_calls;
    if (frames > batch_limit) frames = batch_limit;
    for (size_t i = 0; i < frames; ++i) collect(data[2*i], data[2*i+1]);
    return frames;
}

static void collect_single(int16_t left, int16_t right)
{
    ++single_calls;
    collect(left, right);
}

static void clear_capture(void)
{
    captured_frames = batch_calls = single_calls = 0;
    memset(captured, 0, sizeof(captured));
}

static void set_tone(uint8_t a, uint8_t b, uint8_t enabled)
{
    mem_write(0x23e, 0x03); /* firmware's highest volume setting */
    mem_write(0x22c, a);
    mem_write(0x22d, b);
    mem_write(0x23f, enabled);
}

static uint64_t energy(void)
{
    uint64_t result = 0;
    for (size_t i = 0; i < captured_frames; ++i) {
        int32_t sample = captured[i*2];
        result += sample * sample;
    }
    return result;
}

static void output(void)
{
    fixture_start();
    retro_set_audio_sample_batch(collect_batch);
    retro_set_audio_sample(collect_single);
    fixture_frames(1);
    assert(captured_frames == FRAME_SAMPLES);
    assert(batch_calls == 1 && single_calls == 0);
    assert(energy() == 0);
    clear_capture();
    set_tone(0x67, 0, 0x40);
    fixture_frames(1);
    assert(captured_frames == FRAME_SAMPLES && energy() > 1000000);
    clear_capture();
    retro_set_audio_sample_batch(NULL);
    fixture_frames(1);
    assert(captured_frames == FRAME_SAMPLES && single_calls == FRAME_SAMPLES);
    clear_capture();
    retro_set_audio_sample_batch(collect_batch);
    batch_limit = 128;
    fixture_frames(1);
    assert(captured_frames == FRAME_SAMPLES && batch_calls == 6 && single_calls == 0);
    clear_capture();
    batch_limit = 0;
    fixture_frames(1);
    assert(batch_calls == 1); /* stalled frontends cannot block emulation */
    assert(single_calls == FRAME_SAMPLES);
    retro_set_audio_sample(NULL);
    retro_set_audio_sample_batch(NULL);
    fixture_frames(1);
}

static void timer(void)
{
    fixture_start();
    set_tone(0x67, 0, 0x40);
    mem_write(0x22b, 0xf0);
    mem_write(_TIER, 0); /* masking interrupts must not stop the peripheral */
    sys.ram[_SYSCON] |= 8;
    fixture_frames(7);
    assert(!(sys.ram[_TISR] & 0x20));
    fixture_frames(1);
    assert(sys.ram[_TISR] & 0x20);
    assert(sys_halt_p());
    mem_write(_TISR, 0xdf);
    fixture_frames(6);
    assert(!(sys.ram[_TISR] & 0x20));
    fixture_frames(1);
    assert(sys.ram[_TISR] & 0x20);
    /* A fresh project-owned IRQ handler proves actual vector dispatch/wakeup. */
    const uint8_t irq[] = {0xee,0x12,0x20,0xa9,0xdf,0x85,0x05,0x40};
    memcpy(sys.rom_e + 0x1fff44, irq, sizeof(irq));
    sys.cpu.status &= ~4;
    mem_write(_TIER, 0x20);
    fixture_frames(1);
    assert(sys.ram[0x2012] > 0 && !sys_halt_p());
}

static unsigned rising_edges(void)
{
    unsigned result = 0;
    for (size_t i = 1; i < captured_frames; ++i)
        if (captured[i*2-2] <= 0 && captured[i*2] > 0) ++result;
    return result;
}

static void controls(void)
{
    fixture_start();
    retro_set_audio_sample_batch(collect_batch);
    set_tone(0x67, 0xad, 0x40);
    fixture_frames(60);
    unsigned lower = rising_edges();
    assert(lower >= 263 && lower <= 265);
    uint64_t loud = energy();
    clear_capture();
    set_tone(0x67, 0xad, 0x80);
    fixture_frames(60);
    unsigned higher = rising_edges();
    assert(higher >= 527 && higher <= 529);
    assert(higher >= lower * 2 - 1 && higher <= lower * 2 + 1);
    clear_capture();
    mem_write(0x23e, 0xe3);
    fixture_frames(60);
    assert(energy() > 0 && energy() < loud / 100);
    clear_capture();
    set_tone(0x67, 0xad, 0xc0);
    fixture_frames(60);
    int64_t dc = 0;
    for (size_t i = 0; i < captured_frames; ++i) {
        int32_t sample = captured[i*2];
        assert(sample >= -16384 && sample <= 16384);
        dc += sample;
    }
    assert(dc > -1000000 && dc < 1000000 && energy() > loud);
    clear_capture();
    set_tone(0, 0, 0xc0);
    fixture_frames(1);
    assert(energy() == 0);
    clear_capture();
    set_tone(0x67, 0xad, 0);
    fixture_frames(1);
    assert(energy() == 0);
}

static void replay(void)
{
    fixture_start();
    retro_set_audio_sample_batch(collect_batch);
    set_tone(0x67, 0xad, 0xc0);
    mem_write(0x22b, 0xd1);
    fixture_frames(7);
    size_t size = retro_serialize_size();
    void *saved = malloc(size);
    int16_t *expected = malloc(sizeof(captured));
    assert(saved && expected && retro_serialize(saved, size));
    clear_capture();
    fixture_frames(60);
    assert(captured_frames == SAMPLE_RATE);
    memcpy(expected, captured, sizeof(captured));
    retro_deinit();
    fixture_start();
    assert(retro_unserialize(saved, size));
    /* Serializing while paused cannot advance phase or retain output samples. */
    void *paused = malloc(size);
    assert(paused && retro_serialize(paused, size) && !memcmp(saved, paused, size));
    clear_capture();
    fixture_frames(60);
    assert(captured_frames == SAMPLE_RATE && !memcmp(expected, captured, sizeof(captured)));
    /* CPU and game-delay clock overrides must leave the melody clock unchanged. */
    const float rates[] = {0.25f, 1, 8};
    for (unsigned i = 0; i < 3; ++i) {
        assert(retro_unserialize(saved, size));
        vars.cpu_rate = rates[i]; vars.timer_rate = rates[2-i];
        clear_capture(); fixture_frames(60);
        assert(captured_frames == SAMPLE_RATE && !memcmp(expected, captured, sizeof(captured)));
    }
    retro_reset();
    clear_capture(); fixture_frames(1);
    assert(energy() == 0);
    free(paused); free(saved); free(expected);
}

static void state_versions(void)
{
    fixture_start();
    retro_set_audio_sample_batch(collect_batch);
    set_tone(0x67, 0xad, 0xc0);
    fixture_frames(7);
    size_t size = retro_serialize_size();
    struct retrom_state_v2 *saved = malloc(size);
    assert(saved && retro_serialize(saved, size));
    assert(!memcmp(saved->core.magic, "BBKST002", 8));
    uint8_t marker = sys.ram[0x2011];
    saved->tone_phase[0] = MELODY_MAX_PHASE;
    saved->checksum = state_checksum(saved, offsetof(struct retrom_state_v2, checksum));
    assert(!retro_unserialize(saved, size));
    assert(sys.ram[0x2011] == marker);
    struct retrom_state *old = &saved->core;
    memcpy(old->magic, "BBKST001", 8);
    old->checksum = state_checksum(old, offsetof(struct retrom_state, checksum));
    assert(retro_unserialize(old, sizeof(*old)));
    assert(sys.ram[0x2011] == marker);
    assert(melody.phase[0] == 0 && melody.phase[1] == 0 && melody.timer_phase == 0);
    assert(melody.timer == old->ram[_MTCT]);
    clear_capture(); fixture_frames(1);
    assert(captured_frames == FRAME_SAMPLES && energy() > 0);
    old->ram[0x100] ^= 1;
    assert(!retro_unserialize(old, sizeof(*old)));
    free(saved);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    fixture_create();
    if (!strcmp(argv[1], "audio-output")) output();
    else if (!strcmp(argv[1], "melody-timer")) timer();
    else if (!strcmp(argv[1], "audio-controls")) controls();
    else if (!strcmp(argv[1], "audio-replay")) replay();
    else if (!strcmp(argv[1], "audio-state-versions")) state_versions();
    else return 2;
    fixture_destroy();
    puts(argv[1]);
    return 0;
}
