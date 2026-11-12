#include "aarambh/gfx/font.h"

#include "gfx/font_check.h"
#include "gfx/glyph.h"
#include "lib/memory.h"

#define FIRST_GLYPH ' '
#define LAST_GLYPH  '~'

/*
 * stb_truetype rasterizes a bitmap up to 64 pixels wide with a scanline on
 * the stack; a wider one needs an allocation it does not check. Glyphs are
 * therefore drawn in strips of at most this width.
 */
#define STRIP_WIDTH 64
#define FLATNESS    0.35f

/* No text Aarambh draws needs a larger glyph; a font that asks for one is wrong. */
#define GLYPH_SIZE_MAX 2048

/* Enough bytes for stb to tell a font from a collection and find font 0. */
#define FONT_MIN_SIZE 16

int font_init(Font *font, unsigned char *data, int size)
{
    font->data = data;
    font->loaded = 0;

    if (data != NULL && size >= FONT_MIN_SIZE) {
        int offset = stbtt_GetFontOffsetForIndex(data, 0);

        if (font_check(data, size, offset)) {
            font->loaded = stbtt_InitFont(&font->info, data, offset);
        }
    }

    return font->loaded;
}

void face_init(Face *face, Font *font, float pixel_height)
{
    int ascent;
    int descent;
    int line_gap;

    mem_set(face, 0, sizeof(*face));
    face->font = font;

    if (!font->loaded) {
        return;
    }

    face->scale = stbtt_ScaleForPixelHeight(&font->info, pixel_height);
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);

    face->ascent = (int)(ascent * face->scale);
    face->descent = (int)(-descent * face->scale);
}

int face_line_height(const Face *face)
{
    return face->ascent + face->descent;
}

int face_is_ready(const Face *face)
{
    return face->font != NULL && face->font->loaded;
}

/* The glyph a character maps to, or glyph 0 when the font maps it outside. */
static int glyph_index(const stbtt_fontinfo *info, char character)
{
    int index = stbtt_FindGlyphIndex(info, character);

    if (index < 0 || index >= info->numGlyphs) {
        return 0;
    }
    return index;
}

static void rasterize_strips(const stbtt_fontinfo *info, Glyph *glyph, stbtt_vertex *vertices,
                             int vertex_count, float scale)
{
    int x;

    for (x = 0; x < glyph->width; x += STRIP_WIDTH) {
        stbtt__bitmap strip;

        strip.w = glyph->width - x < STRIP_WIDTH ? glyph->width - x : STRIP_WIDTH;
        strip.h = glyph->height;
        strip.stride = glyph->width;
        strip.pixels = glyph->bitmap + x;

        stbtt_Rasterize(&strip, FLATNESS, vertices, vertex_count, scale, scale, 0.0f, 0.0f,
                        glyph->x_offset + x, glyph->y_offset, 1, info->userdata);
    }
}

/*
 * Draws the glyph's bitmap. Returns 0 when memory ran out: the glyph then has
 * no bitmap and no size, so it is skipped when drawn, and it is tried again
 * the next time it is needed.
 */
static int render_glyph(const stbtt_fontinfo *info, Glyph *glyph, int index, float scale)
{
    stbtt_vertex *vertices;
    int vertex_count = stbtt_GetGlyphShape(info, index, &vertices);
    int x0;
    int y0;
    int x1;
    int y1;
    mem_size_t size;

    glyph->bitmap = NULL;
    glyph->width = 0;
    glyph->height = 0;

    if (vertex_count == 0) {
        return stbtt_IsGlyphEmpty(info, index);
    }

    stbtt_GetGlyphBitmapBox(info, index, scale, scale, &x0, &y0, &x1, &y1);
    glyph->x_offset = x0;
    glyph->y_offset = y0;

    if (x1 <= x0 || y1 <= y0 || x1 - x0 > GLYPH_SIZE_MAX || y1 - y0 > GLYPH_SIZE_MAX) {
        stbtt_FreeShape(info, vertices);
        return 1;
    }

    size = (mem_size_t)(x1 - x0) * (mem_size_t)(y1 - y0);
    glyph->bitmap = mem_alloc(size);

    if (glyph->bitmap != NULL) {
        mem_set(glyph->bitmap, 0, size);
        glyph->width = x1 - x0;
        glyph->height = y1 - y0;
        rasterize_strips(info, glyph, vertices, vertex_count, scale);
    }

    stbtt_FreeShape(info, vertices);
    return glyph->bitmap != NULL;
}

Glyph *face_glyph(Face *face, char character)
{
    const stbtt_fontinfo *info = &face->font->info;
    Glyph *glyph;
    int index;
    int advance;
    int left_bearing;

    if (character < FIRST_GLYPH || character > LAST_GLYPH) {
        character = '?';
    }

    glyph = &face->glyphs[character - FIRST_GLYPH];
    if (glyph->ready) {
        return glyph;
    }

    index = glyph_index(info, character);
    stbtt_GetGlyphHMetrics(info, index, &advance, &left_bearing);
    glyph->advance = advance * face->scale;
    glyph->ready = render_glyph(info, glyph, index, face->scale);

    return glyph;
}

float text_width(Face *face, const char *text)
{
    float width = 0;

    if (!face_is_ready(face)) {
        return 0;
    }

    while (*text != '\0') {
        width += face_glyph(face, *text)->advance;
        text++;
    }

    return width;
}
