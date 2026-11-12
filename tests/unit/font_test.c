/*
 * A damaged font must be refused before stb_truetype reads it; with the
 * sanitizers on, any read outside the file ends the test.
 */

#include <stdlib.h>
#include <string.h>

#include "aarambh/gfx/font.h"
#include "support/efi_mock.h"
#include "support/test.h"

#define FONT_DIR "assets/fonts/"
#define MUTATIONS 400

static unsigned char *read_font(const char *path, int *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char *data;

    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *size = (int)ftell(file);
    fseek(file, 0, SEEK_SET);

    data = malloc((size_t)*size);
    if (fread(data, 1, (size_t)*size, file) != (size_t)*size) {
        *size = 0;
    }

    fclose(file);
    return data;
}

/* The copy is exactly `size` bytes, so the sanitizer sees every byte too far. */
static int loads(const unsigned char *bytes, int size)
{
    unsigned char *copy = malloc(size > 0 ? (size_t)size : 1);
    Font font;
    int loaded;

    if (size > 0) {
        memcpy(copy, bytes, (size_t)size);
    }

    loaded = font_init(&font, copy, size);
    free(copy);
    return loaded;
}

static void draw_all_glyphs(Font *font)
{
    Face face;
    char text[2] = { ' ', '\0' };

    face_init(&face, font, 32.0f);
    for (text[0] = ' '; text[0] <= '~'; text[0]++) {
        text_width(&face, text);
    }
}

static void check_bundled_font(const char *path)
{
    unsigned char *data;
    Font font;
    int size = 0;

    data = read_font(path, &size);
    CHECK(data != NULL && font_init(&font, data, size), path);

    if (data != NULL && font.loaded) {
        draw_all_glyphs(&font);
    }

    free(data);
}

static void check_truncated(const unsigned char *data, int size)
{
    int accepted = 0;
    int length;

    for (length = 0; length < size; length += 499) {
        accepted += loads(data, length);
    }

    CHECK(accepted == 0, "a font cut short at any length is refused");
}

static unsigned int next_random(unsigned int *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

/*
 * One changed byte anywhere in the file. A copy that still loads must draw
 * every glyph without reading or writing outside its buffers.
 */
static void check_damaged_bytes(const unsigned char *data, int size, int from, int span,
                                const char *description)
{
    unsigned char *copy = malloc((size_t)size);
    unsigned int state = 2026;
    int i;

    for (i = 0; i < MUTATIONS; i++) {
        Font font;
        int at = from + (int)(next_random(&state) % (unsigned int)span);

        memcpy(copy, data, (size_t)size);
        copy[at] ^= (unsigned char)(1 + next_random(&state) % 255);

        if (font_init(&font, copy, size)) {
            draw_all_glyphs(&font);
        }
    }

    free(copy);
    CHECK(1, description);
}

static void check_table_past_the_end(const unsigned char *data, int size)
{
    unsigned char *copy = malloc((size_t)size);

    memcpy(copy, data, (size_t)size);
    memset(copy + 12 + 8, 0x7f, 4);

    CHECK(!loads(copy, size), "a table that starts past the end of the file is refused");
    free(copy);
}

int main(void)
{
    static const unsigned char garbage[16] = { 0x13, 0x37, 0xde, 0xad, 0xbe, 0xef };
    unsigned char *regular;
    int size = 0;

    mock_efi_init();

    CHECK(!loads(garbage, 0), "an empty file is refused");
    CHECK(!loads(garbage, 1), "a one byte file is refused");
    CHECK(!loads(garbage, sizeof(garbage)), "16 bytes that are not a font are refused");
    CHECK(!loads((const unsigned char *)"\0\1\0\0\0\xff\0\0\0\0\0\0\0\0\0\0", 16),
          "a header that claims 255 tables in 16 bytes is refused");

    check_bundled_font(FONT_DIR "light.ttf");
    check_bundled_font(FONT_DIR "regular.ttf");
    check_bundled_font(FONT_DIR "bold.ttf");
    check_bundled_font(FONT_DIR "mono.ttf");

    regular = read_font(FONT_DIR "regular.ttf", &size);
    if (regular != NULL && size > 0) {
        check_truncated(regular, size);
        check_table_past_the_end(regular, size);
        check_damaged_bytes(regular, size, 12, ((regular[4] << 8) | regular[5]) * 16,
                            "a damaged table directory never reads outside the file");
        check_damaged_bytes(regular, size, 0, size,
                            "a damaged byte anywhere never reads outside the file");
    }
    free(regular);

    return test_result();
}
