#ifndef AARAMBH_GFX_FONT_H
#define AARAMBH_GFX_FONT_H

#include "stb_config.h"
#include "stb_truetype.h"

#include "aarambh/gfx/image.h"

#define FONT_GLYPH_COUNT 95

typedef struct {
    stbtt_fontinfo info;
    unsigned char *data;
    int loaded;
} Font;

typedef struct {
    unsigned char *bitmap;
    int width;
    int height;
    int x_offset;
    int y_offset;
    float advance;
    int ready;
} Glyph;

typedef struct {
    Font *font;
    float scale;
    int ascent;
    int descent;
    Glyph glyphs[FONT_GLYPH_COUNT];
} Face;

/* `data` must stay allocated while the font is used; a damaged font is refused. */
int font_init(Font *font, unsigned char *data, int size);

void face_init(Face *face, Font *font, float pixel_height);
int face_line_height(const Face *face);
int face_is_ready(const Face *face);

float text_width(Face *face, const char *text);

void draw_text(Image *destination, Face *face, float x, int baseline,
               const char *text, UINT32 color, int alpha);

void draw_text_shadow(Image *destination, Face *face, float x, int baseline,
                      const char *text, UINT32 color, int alpha);

void draw_text_vertical(Image *destination, Face *face, int x, int bottom,
                        const char *text, UINT32 color, int alpha);

#endif
