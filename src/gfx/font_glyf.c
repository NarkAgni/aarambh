#include "gfx/font_table.h"
#include "lib/memory.h"

#define GLYPH_HEADER_SIZE 10

#define POINT_ON_CURVE   0x01
#define POINT_X_SHORT    0x02
#define POINT_Y_SHORT    0x04
#define POINT_REPEAT     0x08
#define POINT_X_SAME     0x10
#define POINT_Y_SAME     0x20

#define PART_WORD_ARGS   0x0001
#define PART_XY_VALUES   0x0002
#define PART_SCALE       0x0008
#define PART_MORE        0x0020
#define PART_XY_SCALE    0x0040
#define PART_TWO_BY_TWO  0x0080

/* The deepest nesting of composite glyphs stb_truetype is asked to follow. */
#define MAX_NESTING      8

#define HEIGHT_UNKNOWN   0
#define HEIGHT_VISITING  0xff

typedef struct {
    const FontTable *loca;
    const FontTable *glyf;
    UINT32 glyphs;
    int long_offsets;
    unsigned char *heights;
} GlyphSet;

static UINT32 glyph_start(const GlyphSet *set, UINT32 index)
{
    if (set->long_offsets) {
        return font_read32(set->loca->data + index * 4);
    }
    return font_read16(set->loca->data + index * 2) * 2;
}

static UINT32 coordinate_size(UINT32 flags, UINT32 short_flag, UINT32 same_flag)
{
    if (flags & short_flag) {
        return 1;
    }
    return (flags & same_flag) ? 0 : 2;
}

/*
 * stb reads the flags, then the x and the y coordinates of every point with
 * no bounds of its own. A contour of a single off-curve point makes it read
 * the point after the last one.
 */
static int check_simple(const unsigned char *glyph, UINT32 length, UINT32 contours)
{
    const unsigned char *ends = glyph + GLYPH_HEADER_SIZE;
    UINT32 at = GLYPH_HEADER_SIZE + 2 * contours;
    UINT32 points;
    UINT32 coordinates = 0;
    UINT32 contour = 0;
    UINT32 contour_start = 0;
    UINT32 point = 0;
    UINT32 i;

    if (at + 2 > length) {
        return 0;
    }

    for (i = 1; i < contours; i++) {
        if (font_read16(ends + 2 * i) <= font_read16(ends + 2 * i - 2)) {
            return 0;
        }
    }

    points = font_read16(ends + 2 * contours - 2) + 1;
    at += 2 + font_read16(glyph + at);

    while (point < points) {
        UINT32 flags;
        UINT32 repeat = 0;

        if (at >= length) {
            return 0;
        }
        flags = glyph[at++];

        if (flags & POINT_REPEAT) {
            if (at >= length) {
                return 0;
            }
            repeat = glyph[at++];
        }

        for (i = 0; i <= repeat && point < points; i++, point++) {
            UINT32 end = font_read16(ends + 2 * contour);

            if (point == contour_start && point == end && (flags & POINT_ON_CURVE) == 0) {
                return 0;
            }

            if (point == end) {
                contour++;
                contour_start = point + 1;
            }

            coordinates += coordinate_size(flags, POINT_X_SHORT, POINT_X_SAME)
                         + coordinate_size(flags, POINT_Y_SHORT, POINT_Y_SAME);
        }
    }

    return (UINT64)at + coordinates <= length;
}

static UINT32 part_size(UINT32 flags)
{
    UINT32 size = 4 + ((flags & PART_WORD_ARGS) ? 4 : 2);

    if (flags & PART_SCALE) {
        size += 2;
    } else if (flags & PART_XY_SCALE) {
        size += 4;
    } else if (flags & PART_TWO_BY_TWO) {
        size += 8;
    }

    return size;
}

static int glyph_height(GlyphSet *set, UINT32 index, UINT32 depth);

/*
 * stb reads each part without bounds and follows it into the glyph it names,
 * so a part must fit, place its glyph by offset (stb has no point matching)
 * and name glyphs that end within the nesting limit, without loops.
 */
static int check_composite(GlyphSet *set, const unsigned char *glyph, UINT32 length,
                           UINT32 depth)
{
    UINT32 at = GLYPH_HEADER_SIZE;
    int height = 0;
    UINT32 flags;

    do {
        UINT32 part;
        int part_height;

        if (at + 4 > length) {
            return -1;
        }

        flags = font_read16(glyph + at);
        part = font_read16(glyph + at + 2);

        if ((flags & PART_XY_VALUES) == 0 || at + part_size(flags) > length) {
            return -1;
        }
        at += part_size(flags);

        if (part < set->glyphs) {
            part_height = glyph_height(set, part, depth + 1);
            if (part_height < 0) {
                return -1;
            }
            if (part_height + 1 > height) {
                height = part_height + 1;
            }
        }
    } while (flags & PART_MORE);

    return height;
}

/*
 * How deep the composites below a glyph go, or -1 for a glyph that cannot be
 * read safely. Each glyph is checked once; `depth` is how deep the caller is.
 */
static int glyph_height(GlyphSet *set, UINT32 index, UINT32 depth)
{
    UINT32 start = glyph_start(set, index);
    UINT32 length = glyph_start(set, index + 1) - start;
    const unsigned char *glyph = set->glyf->data + start;
    int contours;
    int height;

    if (set->heights[index] == HEIGHT_VISITING || depth > MAX_NESTING) {
        return -1;
    }

    if (set->heights[index] != HEIGHT_UNKNOWN) {
        height = set->heights[index] - 1;
        return depth + (UINT32)height > MAX_NESTING ? -1 : height;
    }

    if (length == 0) {
        height = 0;
    } else if (length < GLYPH_HEADER_SIZE) {
        return -1;
    } else {
        contours = font_read_signed16(glyph);

        if (contours >= 0) {
            height = (contours == 0 || check_simple(glyph, length, (UINT32)contours)) ? 0 : -1;
        } else {
            set->heights[index] = HEIGHT_VISITING;
            height = check_composite(set, glyph, length, depth);
        }
    }

    if (height < 0 || depth + (UINT32)height > MAX_NESTING) {
        set->heights[index] = HEIGHT_VISITING;
        return -1;
    }

    set->heights[index] = (unsigned char)(height + 1);
    return height;
}

int font_check_glyphs(const FontTable *loca, const FontTable *glyf, UINT32 glyphs,
                      int long_offsets)
{
    GlyphSet set;
    UINT32 i;
    int intact = 1;

    set.loca = loca;
    set.glyf = glyf;
    set.glyphs = glyphs;
    set.long_offsets = long_offsets;
    set.heights = mem_alloc(glyphs > 0 ? glyphs : 1);

    if (set.heights == NULL) {
        return 0;
    }

    mem_set(set.heights, HEIGHT_UNKNOWN, glyphs > 0 ? glyphs : 1);

    for (i = 0; i < glyphs && intact; i++) {
        intact = glyph_height(&set, i, 0) >= 0;
    }

    mem_free(set.heights);
    return intact;
}
