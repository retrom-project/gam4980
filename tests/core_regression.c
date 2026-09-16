#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../src/libretro.c"

static bool environment(unsigned command, void *data)
{
    (void)command;
    (void)data;
    return false;
}

static void malformed_game(void)
{
    uint8_t bytes[0x100] = {0};
    struct retro_game_info game = {.data = bytes, .size = 1};
    assert(!retro_load_game(&game));
    game.size = sizeof(bytes);
    assert(!retro_load_game(&game));
}

static void state_bounds(void)
{
    uint8_t sentinel = 0x5a;
    assert(!retro_serialize(&sentinel, 0));
    assert(sentinel == 0x5a);
    assert(!retro_unserialize(&sentinel, 0));
    assert(!retro_serialize(NULL, retro_serialize_size()));
    assert(!retro_unserialize(NULL, retro_serialize_size()));
}

static void flash_roundtrip(void)
{
    size_t size = retro_serialize_size();
    void *saved = malloc(size);
    assert(saved);
    mem_init();
    sys.flash[0] = 0x42;
    sys.flash[0x1fffff] = 0x91;
    sys.ram[0x2010] = 0x73;
    assert(retro_serialize(saved, size));
    sys.flash[0] = 0;
    sys.flash[0x1fffff] = 0;
    sys.ram[0x2010] = 0;
    assert(retro_unserialize(saved, size));
    assert(sys.flash[0] == 0x42);
    assert(sys.flash[0x1fffff] == 0x91);
    assert(sys.ram[0x2010] == 0x73);
    free(saved);
}

static void corrupt_state(void)
{
    size_t size = retro_serialize_size();
    uint8_t *saved = malloc(size);
    assert(saved);
    assert(retro_serialize(saved, size));
    saved[size / 2] ^= 0x20;
    sys.ram[0x2010] = 0x71;
    assert(!retro_unserialize(saved, size));
    assert(sys.ram[0x2010] == 0x71);
    free(saved);
}

static void keyboard_bounds(void)
{
    keyboard_cb(true, UINT_MAX, 0, 0);
    keyboard_cb(true, RETROK_LAST, 0, 0);
    sys.ram[_KEYCODE] = 0;
    keyboard_cb(false, RETROK_RETURN, 0, 0);
    assert(sys.ram[_KEYCODE] == 0);
    keyboard_cb(true, RETROK_RETURN, 0, 0);
    assert(sys.ram[_KEYCODE] == (KEY_ENTER | 0x80));
}

static void timer_remainder(void)
{
    sys.ram[_STCON] = 1;
    sys.ram[_ST1LD] = 166;
    sys.ram[_TIER] = 1;
    timing.timers[0] = 240;
    sys_timer(32);
    assert(timing.timers[0] == 182);
    assert(sys.ram[_TISR] & 1);
    sys_timer(180);
    assert(timing.timers[0] == 182);
}

static void timer_cadence(void)
{
    /* A halted CPU still clocks peripherals; frame boundaries must not lose time. */
    sys.ram[_SYSCON] = 8;
    sys.ram[_STCON] = 1;
    vars.cpu_rate = vars.timer_rate = 1;
    for (unsigned i = 0; i < 60; ++i)
        sys_step();
    assert(timing.timers[0] == 15); /* floor(60 * floor(4MHz / 60) / 400) %% 256 */
    assert(timing.ticked == 360);
}

static void cpu_slice(void)
{
    mem_init();
    memset(sys.ram + 0x400, 0xea, 0x400); /* project-owned straight-line NOPs */
    sys.ram[0x800] = 0x4c;
    sys.ram[0x801] = 0;
    sys.ram[0x802] = 4;
    sys.cpu.pc = 0x400;
    assert(s6502_exec(&sys.cpu, 256) == 256);
    assert(sys.cpu.pc == 0x480);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    environ_cb = environment;
    if (!strcmp(argv[1], "malformed-game")) malformed_game();
    else if (!strcmp(argv[1], "state-bounds")) state_bounds();
    else if (!strcmp(argv[1], "flash-roundtrip")) flash_roundtrip();
    else if (!strcmp(argv[1], "corrupt-state")) corrupt_state();
    else if (!strcmp(argv[1], "keyboard-bounds")) keyboard_bounds();
    else if (!strcmp(argv[1], "timer-remainder")) timer_remainder();
    else if (!strcmp(argv[1], "timer-cadence")) timer_cadence();
    else if (!strcmp(argv[1], "cpu-slice")) cpu_slice();
    else return 2;
    puts(argv[1]);
    return 0;
}
