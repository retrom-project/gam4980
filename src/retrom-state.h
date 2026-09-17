/* Complete, bounded little-endian instant state. No process pointers or BIOS bytes. */
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "The Retrom state ABI currently requires a little-endian target"
#endif

struct __attribute__((packed)) retrom_state {
    uint8_t magic[8];
    uint64_t game_identity;
    uint64_t bios_identity;
    uint8_t ram[0x8000];
    uint8_t flash[0x200000];
    uint16_t pc;
    uint8_t ac, ix, iy, sp, status;
    uint8_t bk_sel;
    uint16_t bk_tab[16];
    uint16_t bk_sys_d;
    uint8_t flash_cmd, flash_cycles;
    uint32_t timers[5];
    int32_t cycles;
    uint32_t ticked, rtc_usec;
    uint64_t emulated_usec, last_input_usec;
    uint8_t last_input_key;
    int32_t pressed;
    uint32_t repeat;
    int8_t ghost[(LCD_WIDTH + 1) * LCD_HEIGHT];
    uint16_t pixels[(LCD_WIDTH + 1) * LCD_HEIGHT];
    float cpu_rate, timer_rate;
    uint16_t lcd_bg, lcd_fg;
    uint8_t lcd_ghosting;
    uint32_t key_interval;
    uint32_t checksum;
};

/* Keep the exact v1 layout above for a bounded backward reader. v2 wraps it
   and adds peripheral state. The old checksum slot is reserved/zero in v2. */
struct __attribute__((packed)) retrom_state_v2 {
    struct retrom_state core;
    uint32_t tone_phase[2];
    uint32_t melody_timer_phase;
    uint16_t melody_timer;
    uint16_t reserved;
    uint32_t checksum;
};

static uint32_t state_checksum(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

size_t retro_serialize_size(void)
{
    return sizeof(struct retrom_state_v2);
}

bool retro_serialize(void *data, size_t size)
{
    if (!data || size < sizeof(struct retrom_state_v2))
        return false;
    struct retrom_state_v2 *snapshot = calloc(1, sizeof(*snapshot));
    if (!snapshot)
        return false;
    struct retrom_state *state = &snapshot->core;
    memcpy(state->magic, "BBKST002", 8);
    state->game_identity = game_identity;
    state->bios_identity = bios_identity;
    memcpy(state->ram, sys.ram, sizeof(sys.ram));
    memcpy(state->flash, sys.flash, sizeof(sys.flash));
    state->pc = sys.cpu.pc;
    state->ac = sys.cpu.ac;
    state->ix = sys.cpu.ix;
    state->iy = sys.cpu.iy;
    state->sp = sys.cpu.sp;
    state->status = sys.cpu.status;
    state->bk_sel = sys.bk_sel;
    memcpy(state->bk_tab, sys.bk_tab, sizeof(sys.bk_tab));
    state->bk_sys_d = sys.bk_sys_d;
    state->flash_cmd = sys.flash_cmd;
    state->flash_cycles = sys.flash_cycles;
    memcpy(state->timers, timing.timers, sizeof(timing.timers));
    state->cycles = timing.cycles;
    state->ticked = timing.ticked;
    state->rtc_usec = timing.rtc_usec;
    state->emulated_usec = timing.emulated_usec;
    state->last_input_usec = timing.last_input_usec;
    state->last_input_key = timing.last_input_key;
    state->pressed = timing.pressed;
    state->repeat = timing.repeat;
    memcpy(state->ghost, fa, sizeof(fa));
    memcpy(state->pixels, fb, sizeof(fb));
    state->cpu_rate = vars.cpu_rate;
    state->timer_rate = vars.timer_rate;
    state->lcd_bg = vars.lcd_bg;
    state->lcd_fg = vars.lcd_fg;
    state->lcd_ghosting = vars.lcd_ghosting;
    state->key_interval = vars.key_pressed_input_min_interval;
    memcpy(snapshot->tone_phase, melody.phase, sizeof(melody.phase));
    snapshot->melody_timer_phase = melody.timer_phase;
    snapshot->melody_timer = melody.timer;
    snapshot->checksum = state_checksum(snapshot, offsetof(struct retrom_state_v2, checksum));
    memcpy(data, snapshot, sizeof(*snapshot));
    free(snapshot);
    return true;
}

static bool valid_state(const struct retrom_state *state)
{
    if (state->game_identity != game_identity || state->bios_identity != bios_identity ||
        state->bk_sel > 15 || state->bk_sys_d != sys.bk_sys_d ||
        state->flash_cmd > 3 || state->flash_cycles > 5 ||
        state->rtc_usec >= 1000000 || state->last_input_usec > state->emulated_usec ||
        state->last_input_key > 0x3b || state->pressed < -1 || state->pressed > 15 ||
        state->repeat > 20 || state->cycles < -1000000 || state->cycles > 1000000 ||
        state->ticked >= 12800 || state->lcd_ghosting > 40 || state->key_interval > 500 ||
        !(state->cpu_rate >= 0.25f && state->cpu_rate <= 8.0f) ||
        !(state->timer_rate >= 0.25f && state->timer_rate <= 8.0f))
        return false;
    for (unsigned i = 0; i < 16; ++i)
        if (state->bk_tab[i] > 0x0fff)
            return false;
    for (unsigned i = 0; i < 5; ++i)
        if (state->timers[i] >= (i == 4 ? 0x1000u : 0x100u))
            return false;
    return true;
}

bool retro_unserialize(const void *data, size_t size)
{
    if (!data || (size != sizeof(struct retrom_state) && size != sizeof(struct retrom_state_v2)))
        return false;
    struct retrom_state_v2 *snapshot = calloc(1, sizeof(*snapshot));
    if (!snapshot)
        return false;
    memcpy(snapshot, data, size);
    struct retrom_state *state = &snapshot->core;
    bool valid;
    if (size == sizeof(struct retrom_state)) {
        valid = !memcmp(state->magic, "BBKST001", 8) &&
            state->checksum == state_checksum(state, offsetof(struct retrom_state, checksum));
        /* v1 never emulated audio. Begin at its saved timer load, with no stale
           samples or invented phase, while preserving all existing machine state. */
        snapshot->melody_timer = state->ram[_MTCT];
    } else {
        valid = !memcmp(state->magic, "BBKST002", 8) && !state->checksum && !snapshot->reserved &&
            snapshot->checksum == state_checksum(snapshot, offsetof(struct retrom_state_v2, checksum)) &&
            snapshot->tone_phase[0] < MELODY_MAX_PHASE && snapshot->tone_phase[1] < MELODY_MAX_PHASE &&
            snapshot->melody_timer_phase < MELODY_SAMPLE_RATE && snapshot->melody_timer < 256;
    }
    if (!valid || !valid_state(state)) {
        free(snapshot);
        return false;
    }
    memcpy(sys.ram, state->ram, sizeof(sys.ram));
    memcpy(sys.flash, state->flash, sizeof(sys.flash));
    sys.cpu.pc = state->pc;
    sys.cpu.ac = state->ac;
    sys.cpu.ix = state->ix;
    sys.cpu.iy = state->iy;
    sys.cpu.sp = state->sp;
    sys.cpu.status = state->status;
    sys.bk_sel = state->bk_sel;
    memcpy(sys.bk_tab, state->bk_tab, sizeof(sys.bk_tab));
    sys.bk_sys_d = state->bk_sys_d;
    sys.flash_cmd = state->flash_cmd;
    sys.flash_cycles = state->flash_cycles;
    memcpy(timing.timers, state->timers, sizeof(timing.timers));
    timing.cycles = state->cycles;
    timing.ticked = state->ticked;
    timing.rtc_usec = state->rtc_usec;
    timing.emulated_usec = state->emulated_usec;
    timing.last_input_usec = state->last_input_usec;
    timing.last_input_key = state->last_input_key;
    timing.pressed = state->pressed;
    timing.repeat = state->repeat;
    memcpy(fa, state->ghost, sizeof(fa));
    memcpy(fb, state->pixels, sizeof(fb));
    vars.cpu_rate = state->cpu_rate;
    vars.timer_rate = state->timer_rate;
    vars.lcd_bg = state->lcd_bg;
    vars.lcd_fg = state->lcd_fg;
    vars.lcd_ghosting = state->lcd_ghosting;
    vars.key_pressed_input_min_interval = state->key_interval;
    mem_init();
    for (unsigned i = 1; i < 16; ++i)
        mem_bs(i);
    if (sys.flash_cmd == 2 || sys.flash_cmd == 3) {
        for (unsigned bank = 1; bank < 16; ++bank)
            if (sys.bk_tab[bank] >= 0x200 && sys.bk_tab[bank] < 0x400)
                for (unsigned page = 0; page < 16; ++page)
                    sys.mem_r[bank * 16 + page] = NULL;
    }
    melody_reset();
    memcpy(melody.phase, snapshot->tone_phase, sizeof(melody.phase));
    melody.timer_phase = snapshot->melody_timer_phase;
    melody.timer = snapshot->melody_timer;
    shutdown_requested = false;
    free(snapshot);
    return true;
}
