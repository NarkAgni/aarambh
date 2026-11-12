/*
 * Memory running out while a glyph is drawn: text must be skipped, never
 * drawn through a missing bitmap, and appear once memory is back.
 */

#include <string.h>

#include "aarambh/gfx/font.h"
#include "aarambh/gfx/image.h"
#include "gfx/glyph.h"
#include "support/efi_mock.h"
#include "support/font_edit.h"
#include "support/test.h"

#define STAGES 80

static Image canvas;

static void clear(void)
{
    memset(canvas.pixels, 0, (size_t)canvas.width * (size_t)canvas.height * 4);
}

static int drawn_pixels(void)
{
    int count = 0;
    int i;

    for (i = 0; i < canvas.width * canvas.height; i++) {
        count += canvas.pixels[i] != 0;
    }
    return count;
}

/* Horizontal, shadowed and rotated text, the three ways themes draw it. */
static void draw_all(Font *font, float size, const char *text)
{
    Face face;

    face_init(&face, font, size);
    draw_text(&canvas, &face, 2.0f, (int)size, text, 0xffffff, 256);
    draw_text_shadow(&canvas, &face, 2.0f, (int)size * 2, text, 0xffffff, 256);
    draw_text_vertical(&canvas, &face, 2, canvas.height - 2, text, 0xffffff, 256);
}

/*
 * Allocation number `stage` and every later one fails, at each point a glyph
 * needs memory: its outline, its bitmap, the rasterizer's own buffers.
 */
static void check_stages(Font *font, float size, const char *text, const char *description)
{
    int stage;

    for (stage = 0; stage < STAGES; stage++) {
        mock_fail_allocations_from(stage);
        draw_all(font, size, text);
        mock_allow_allocations();
    }

    CHECK(1, description);
}

static void check_recovery(Font *font)
{
    Face face;
    Glyph *glyph;

    face_init(&face, font, 120.0f);

    mock_fail_allocations_from(0);
    glyph = face_glyph(&face, 'W');
    mock_allow_allocations();
    CHECK(glyph->bitmap == NULL && glyph->width == 0 && glyph->height == 0 && !glyph->ready
          && glyph->advance > 0,
          "a glyph without memory has no bitmap, no size, its advance, and is tried again");

    clear();
    draw_text(&canvas, &face, 2.0f, 130, "W", 0xffffff, 256);
    CHECK(glyph->ready && glyph->bitmap != NULL && drawn_pixels() > 0,
          "once memory is back the glyph is drawn");
}

int main(void)
{
    FontFile file;
    Font font;

    mock_efi_init();
    image_alloc(&canvas, 900, 700);

    if (!font_file_load(&file, "regular.ttf") || !font_init(&font, file.data, file.size)) {
        CHECK(0, "assets/fonts/regular.ttf loads");
        return test_result();
    }

    mock_fail_allocations_from(0);
    draw_all(&font, 32.0f, "A");
    mock_allow_allocations();
    CHECK(1, "the reported case: drawing 'A' with no memory at all does not crash");

    check_stages(&font, 32.0f, "Ubuntu 6.8", "memory running out at any step of small text is safe");
    check_stages(&font, 200.0f, "WM", "memory running out at any step of glyphs wider than 64 pixels is safe");
    check_recovery(&font);

    mock_fail_allocations_from(0);
    {
        FontFile copy = font_file_copy(&file);
        Font refused;

        CHECK(!font_init(&refused, copy.data, copy.size),
              "a font that cannot be checked for lack of memory is not used");
        font_file_free(&copy);
    }
    mock_allow_allocations();

    font_file_free(&file);
    return test_result();
}
