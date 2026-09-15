/* Project-owned synthetic BIOS and GAM instructions; no third-party ROM data. */
#include <sys/stat.h>
#include <unistd.h>

static char bios_root[] = "/tmp/gam4980-test-XXXXXX";
static uint8_t synthetic_game[0x2000];
static uint16_t joypad;

static bool fixture_environment(unsigned command, void *data)
{
    if (command == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY) {
        *(const char **)data = bios_root;
        return true;
    }
    return false;
}

static void fixture_video(const void *data, unsigned width, unsigned height, size_t pitch)
{
    assert(data && width == 159 && height == 96 && pitch == 320);
}

static void fixture_poll(void) {}

static int16_t fixture_input(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)index;
    return port == 0 && device == RETRO_DEVICE_JOYPAD && id < 16 && (joypad & (1u << id));
}

static void write_fixture_rom(const char *name, const uint8_t *bytes, size_t size)
{
    char path[512];
    assert(snprintf(path, sizeof(path), "%s/gam4980/%s", bios_root, name) > 0);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, size, file) == size);
    assert(!fclose(file));
}

static void fixture_create(void)
{
    assert(mkdtemp(bios_root));
    char directory[512];
    assert(snprintf(directory, sizeof(directory), "%s/gam4980", bios_root) > 0);
    assert(!mkdir(directory, 0700));
    uint8_t *rom = calloc(1, 0x200000);
    assert(rom);
    write_fixture_rom("8.BIN", rom, 0x200000);
    const uint8_t boot[] = {
        0xa9, 0x02, 0x85, 0x0c, 0xa9, 0x02, 0x85, 0x0d, 0xa9, 0x00, 0x85, 0x0e,
        0xa9, 0x0d, 0x85, 0x0c, 0xa9, 0xa8, 0x85, 0x0d, 0xa9, 0x0e, 0x85, 0x0e,
        0xa9, 0xfe, 0x8d, 0x2b, 0x02, 0x4c, 0x6d, 0x03
    };
    memcpy(rom + 0x1fff50, boot, sizeof(boot));
    write_fixture_rom("E.BIN", rom, 0x200000);
    free(rom);
    memcpy(synthetic_game, "GAM\0", 4);
    synthetic_game[0x40] = 0x46;
    synthetic_game[0x41] = 0x50;
    synthetic_game[0x43] = 0x10;
    const uint8_t program[] = {0xad,0x4e,0x02,0x8d,0x10,0x20,0xee,0x11,0x20,0x4c,0x46,0x50};
    memcpy(synthetic_game + 0x46, program, sizeof(program));
    retro_set_environment(fixture_environment);
    retro_set_video_refresh(fixture_video);
    retro_set_input_poll(fixture_poll);
    retro_set_input_state(fixture_input);
}

static void fixture_start(void)
{
    retro_init();
    struct retro_game_info game = {.data = synthetic_game, .size = sizeof(synthetic_game)};
    assert(retro_load_game(&game));
}

static void fixture_frames(unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        frame_cb(16667);
        retro_run();
    }
}

static void fixture_destroy(void)
{
    retro_deinit();
    char path[512];
    snprintf(path, sizeof(path), "%s/gam4980/8.BIN", bios_root);
    assert(!unlink(path));
    snprintf(path, sizeof(path), "%s/gam4980/E.BIN", bios_root);
    assert(!unlink(path));
    snprintf(path, sizeof(path), "%s/gam4980", bios_root);
    assert(!rmdir(path));
    assert(!rmdir(bios_root));
}
