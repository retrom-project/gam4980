#include <assert.h>
#include <stdio.h>
#include "../src/libretro.c"
#include "core_fixture.h"

static void invalid_inputs(void)
{
    fixture_start();
    uint8_t bad[0x100] = {0};
    struct retro_game_info game = {.data = bad, .size = 1};
    assert(!retro_load_game(&game));
    game.size = sizeof(bad);
    assert(!retro_load_game(&game));
    memcpy(bad, synthetic_game, sizeof(bad));
    bad[0x45] = 0xff;
    assert(!retro_load_game(&game));
    game.data = synthetic_game;
    game.size = 0x1e0001;
    assert(!retro_load_game(&game));
    keyboard_cb(true, RETROK_RETURN, 0, 0);
    fixture_frames(2);
    assert(sys.ram[0x2010] == (KEY_ENTER | 0x80));
}

static void cross_instance(void)
{
    fixture_start();
    sys.ram[_STCON] = 1;
    sys.ram[_STCTCON] = 0x50;
    joypad = 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
    fixture_frames(19);
    assert(sys.ram[0x2010] == (KEY_RIGHT | 0x80));
    sys.flash[0x1fffff] = 0x91;
    size_t size = retro_serialize_size();
    uint8_t *saved = malloc(size), *expected = malloc(size), *actual = malloc(size);
    assert(saved && expected && actual);
    assert(retro_serialize(saved, size));
    fixture_frames(37);
    assert(retro_serialize(expected, size));
    assert(memcmp(saved, expected, size));
    retro_deinit();
    fixture_start();
    fixture_frames(3);
    assert(retro_unserialize(saved, size));
    assert(sys.flash[0x1fffff] == 0x91);
    fixture_frames(37);
    assert(retro_serialize(actual, size));
    assert(!memcmp(expected, actual, size));
    joypad = 0;
    fixture_frames(1);
    joypad = 1u << RETRO_DEVICE_ID_JOYPAD_A;
    fixture_frames(1);
    assert(sys.ram[0x2010] == (KEY_ENTER | 0x80));
    joypad = 0;
    retro_reset();
    fixture_frames(2);
    assert(sys.flash[0x1fffff] == 0xff);
    free(saved); free(expected); free(actual);
}

static void incompatible_content(void)
{
    fixture_start();
    size_t size = retro_serialize_size();
    uint8_t *saved = malloc(size);
    assert(saved && retro_serialize(saved, size));
    retro_deinit();
    synthetic_game[0x70] ^= 1;
    fixture_start();
    assert(!retro_unserialize(saved, size));
    free(saved);
}

static void broken_bios(void)
{
    const uint8_t empty = 0;
    write_fixture_rom("8.BIN", &empty, 1);
    retro_init();
    struct retro_game_info game = {.data = synthetic_game, .size = sizeof(synthetic_game)};
    assert(!retro_load_game(&game));
    retro_run();
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    fixture_create();
    if (!strcmp(argv[1], "invalid-inputs")) invalid_inputs();
    else if (!strcmp(argv[1], "cross-instance")) cross_instance();
    else if (!strcmp(argv[1], "incompatible-content")) incompatible_content();
    else if (!strcmp(argv[1], "broken-bios")) broken_bios();
    else return 2;
    fixture_destroy();
    puts(argv[1]);
    return 0;
}
