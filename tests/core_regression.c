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

int main(int argc, char **argv)
{
    assert(argc == 2);
    environ_cb = environment;
    if (!strcmp(argv[1], "malformed-game")) malformed_game();
    else if (!strcmp(argv[1], "state-bounds")) state_bounds();
    else if (!strcmp(argv[1], "flash-roundtrip")) flash_roundtrip();
    else if (!strcmp(argv[1], "corrupt-state")) corrupt_state();
    else if (!strcmp(argv[1], "keyboard-bounds")) keyboard_bounds();
    else return 2;
    puts(argv[1]);
    return 0;
}
