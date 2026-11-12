/*
 * Low memory and damaged fonts while the menu loads: every entry must still
 * end up with a full-screen background, or loading must fail cleanly.
 */

#include <stdlib.h>
#include <string.h>

#include "core/assets.h"
#include "efi/fs.h"
#include "support/efi_mock.h"
#include "support/test.h"

#define SCREEN_WIDTH  64
#define SCREEN_HEIGHT 48
#define ENTRY_COUNT   3

static const char *good_font_name;

/* No image exists on this ESP; one font does when a test names it. */
void *fs_read(const char *name, UINTN *size)
{
    FILE *file;
    unsigned char *data;
    long length;

    if (good_font_name == NULL || strcmp(name, good_font_name) != 0) {
        if (strncmp(name, "fonts/", 6) != 0) {
            return NULL;
        }

        data = mem_alloc(64);
        memset(data, 0x5a, 64);
        *size = 64;
        return data;
    }

    file = fopen("assets/fonts/regular.ttf", "rb");
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);

    data = mem_alloc((mem_size_t)length + 1);
    *size = fread(data, 1, (size_t)length, file);
    fclose(file);
    return data;
}

static void make_entries(EntryList *entries)
{
    int i;

    memset(entries, 0, sizeof(*entries));
    entries->count = ENTRY_COUNT;

    for (i = 0; i < ENTRY_COUNT; i++) {
        entries->items[i].accent = 0x3c6eb4;
        strcpy(entries->items[i].background_name, "backgrounds/missing.jpg");
    }
}

static int full_screen_backgrounds(const EntryList *entries, const Image *screen)
{
    int i;

    for (i = 0; i < entries->count; i++) {
        const Image *background = &entries->items[i].background;

        if (image_is_empty(background) || background->width != screen->width
            || background->height != screen->height) {
            return 0;
        }
    }

    return 1;
}

/* What the themes do with a background on every frame. */
static void draw_backgrounds(const EntryList *entries, Image *screen)
{
    int i;

    for (i = 0; i < entries->count; i++) {
        image_copy(screen, &entries->items[i].background);
        image_blend(screen, screen, &entries->items[i].background, 128);
    }
}

static int shared_backgrounds(const EntryList *entries)
{
    int shared = 0;
    int i;

    for (i = 1; i < entries->count; i++) {
        if (entries->items[i].background.pixels == entries->items[i - 1].background.pixels) {
            shared++;
        }
    }

    return shared;
}

static void check_backgrounds(Image *screen)
{
    EntryList entries;
    int loaded;

    make_entries(&entries);
    loaded = assets_load_entries(&entries, screen);
    CHECK(loaded && full_screen_backgrounds(&entries, screen) && shared_backgrounds(&entries) == 0,
          "enough memory: every entry gets a background of its own");
    draw_backgrounds(&entries, screen);

    make_entries(&entries);
    mock_fail_allocations_from(0);
    loaded = assets_load_entries(&entries, screen);
    mock_allow_allocations();
    CHECK(!loaded, "no memory for a single background: loading fails instead of crashing");

    make_entries(&entries);
    mock_fail_allocations_from(1);
    loaded = assets_load_entries(&entries, screen);
    mock_allow_allocations();
    CHECK(loaded && full_screen_backgrounds(&entries, screen)
          && shared_backgrounds(&entries) == ENTRY_COUNT - 1,
          "memory for one background: every entry shares the spare");
    draw_backgrounds(&entries, screen);

    make_entries(&entries);
    mock_fail_allocations_from(2);
    loaded = assets_load_entries(&entries, screen);
    mock_allow_allocations();
    CHECK(loaded && full_screen_backgrounds(&entries, screen)
          && shared_backgrounds(&entries) == ENTRY_COUNT - 2,
          "memory runs out at a later entry: only the rest share the spare");
    draw_backgrounds(&entries, screen);
}

static void check_empty_images(Image *screen)
{
    Image empty;
    Image cover;

    mock_fail_allocations_from(0);
    CHECK(!image_alloc(&empty, 1920, 1080) && image_is_empty(&empty)
          && empty.width == 0 && empty.height == 0,
          "a failed allocation leaves an image with no size and no pixels");
    mock_allow_allocations();

    image_vignette(&empty);
    image_blend(screen, screen, &empty, 128);
    CHECK(!image_cover(&cover, &empty, 32, 32), "an empty image cannot be scaled");
    CHECK(!image_alloc(&empty, 0, 10) && !image_alloc(&empty, 10, -1),
          "an image without a positive size is refused");
}

static int loaded_fonts(const FontSet *fonts)
{
    return fonts->light.loaded + fonts->regular.loaded + fonts->bold.loaded + fonts->mono.loaded;
}

static void check_fonts(void)
{
    Config config;
    FontSet fonts;
    Face face;

    memset(&config, 0, sizeof(config));
    strcpy(config.font_light, "fonts/light.ttf");
    strcpy(config.font_regular, "fonts/regular.ttf");
    strcpy(config.font_bold, "fonts/bold.ttf");
    strcpy(config.font_mono, "fonts/mono.ttf");

    good_font_name = NULL;
    memset(&fonts, 0, sizeof(fonts));
    assets_load_fonts(&fonts, &config);
    face_init(&face, &fonts.regular, 24.0f);
    CHECK(loaded_fonts(&fonts) == 0 && !face_is_ready(&face) && text_width(&face, "Ubuntu") == 0,
          "four damaged fonts: none is used, text is skipped");

    good_font_name = "fonts/mono.ttf";
    memset(&fonts, 0, sizeof(fonts));
    assets_load_fonts(&fonts, &config);
    face_init(&face, &fonts.bold, 24.0f);
    CHECK(loaded_fonts(&fonts) == 4 && text_width(&face, "Ubuntu") > 0,
          "three damaged fonts: all text uses the one that is intact");
}

int main(void)
{
    Image screen;

    mock_efi_init();
    image_alloc(&screen, SCREEN_WIDTH, SCREEN_HEIGHT);

    check_backgrounds(&screen);
    check_empty_images(&screen);
    check_fonts();

    return test_result();
}
