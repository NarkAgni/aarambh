#include "aarambh/gfx/font.h"

#include "gfx/font_check.h"
#include "gfx/glyph.h"
#include "lib/memory.h"

#define FIRST_GLYPH ' '
#define LAST_GLYPH  '~'

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

Glyph *face_glyph(Face *face, char character)
{
    Glyph *glyph;
    int advance;
    int left_bearing;

    if (character < FIRST_GLYPH || character > LAST_GLYPH) {
        character = '?';
    }

    glyph = &face->glyphs[character - FIRST_GLYPH];
    if (glyph->ready) {
        return glyph;
    }

    glyph->bitmap = stbtt_GetCodepointBitmap(&face->font->info, 0, face->scale, character,
                                             &glyph->width, &glyph->height,
                                             &glyph->x_offset, &glyph->y_offset);

    stbtt_GetCodepointHMetrics(&face->font->info, character, &advance, &left_bearing);
    glyph->advance = advance * face->scale;
    glyph->ready = 1;

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
