/*
 * Fonts that are built wrong but carry correct checksums: the structure
 * checks must refuse every one that would make stb_truetype read outside the
 * file, and accept the correct versions of the same tables. A font that is
 * accepted is drawn completely; with the sanitizers on, any read outside a
 * buffer ends the test.
 */

#include <stdlib.h>
#include <string.h>

#include "aarambh/gfx/font.h"
#include "aarambh/gfx/image.h"
#include "gfx/glyph.h"
#include "support/efi_mock.h"
#include "support/font_edit.h"
#include "support/test.h"

#define FIRST_CHAR   32
#define CHAR_COUNT   95
#define FUZZ_ROUNDS  600

static FontFile original;
static Image canvas;

static void draw_everything(Font *font)
{
    static const float sizes[] = { 12.0f, 40.0f, 150.0f };
    char text[CHAR_COUNT + 1];
    int i;

    for (i = 0; i < CHAR_COUNT; i++) {
        text[i] = (char)(FIRST_CHAR + i);
    }
    text[CHAR_COUNT] = '\0';

    for (i = 0; i < 3; i++) {
        Face face;

        face_init(&face, font, sizes[i]);
        draw_text(&canvas, &face, 4.0f, 200, text, 0xffffff, 256);
        draw_text_vertical(&canvas, &face, 10, 400, text, 0xffffff, 256);
    }
}

/* 1 when the font loads; a loaded font is drawn completely. */
static int loads(const FontFile *file)
{
    FontFile copy = font_file_copy(file);
    Font font;
    int loaded = font_init(&font, copy.data, copy.size);

    if (loaded) {
        draw_everything(&font);
    }

    font_file_free(&copy);
    return loaded;
}

static int with_table(const char *tag, const unsigned char *bytes, int length)
{
    FontFile edited = font_file_copy(&original);
    int loaded;

    font_file_replace_table(&edited, tag, bytes, length);
    loaded = loads(&edited);
    font_file_free(&edited);
    return loaded;
}

/* The glyph ids of the original font for the printable characters. */
static void original_glyphs(unsigned int *glyphs)
{
    FontFile copy = font_file_copy(&original);
    Font font;
    int i;

    font_init(&font, copy.data, copy.size);
    for (i = 0; i < CHAR_COUNT; i++) {
        glyphs[i] = (unsigned int)stbtt_FindGlyphIndex(&font.info, FIRST_CHAR + i);
    }
    font_file_free(&copy);
}

/* cmap header with one encoding record (3,1) pointing right after it. */
static int cmap_header(unsigned char *cmap, unsigned int platform, unsigned int encoding)
{
    memset(cmap, 0, 12);
    font_file_write16(cmap + 2, 1);
    font_file_write16(cmap + 4, platform);
    font_file_write16(cmap + 6, encoding);
    font_file_write32(cmap + 8, 12);
    return 12;
}

static int format6(unsigned char *subtable, const unsigned int *glyphs, unsigned int count)
{
    unsigned int i;

    font_file_write16(subtable, 6);
    font_file_write16(subtable + 2, 10 + 2 * count);
    font_file_write16(subtable + 4, 0);
    font_file_write16(subtable + 6, FIRST_CHAR);
    font_file_write16(subtable + 8, count);
    for (i = 0; i < count; i++) {
        font_file_write16(subtable + 10 + 2 * i, glyphs[i]);
    }
    return (int)(10 + 2 * count);
}

static void check_format6(const unsigned int *glyphs)
{
    unsigned char cmap[512];
    int length;

    length = cmap_header(cmap, 3, 1);
    length += format6(cmap + length, glyphs, CHAR_COUNT);
    CHECK(with_table("cmap", cmap, length), "a correct format 6 map is accepted and drawn");

    length = cmap_header(cmap, 3, 1);
    font_file_write16(cmap + length, 6);
    font_file_write16(cmap + length + 2, 8);
    font_file_write16(cmap + length + 4, 0);
    font_file_write16(cmap + length + 6, FIRST_CHAR);
    CHECK(!with_table("cmap", cmap, length + 8),
          "the reported case: a format 6 map of 8 bytes without its count is refused");

    length = cmap_header(cmap, 3, 1);
    format6(cmap + length, glyphs, CHAR_COUNT);
    font_file_write16(cmap + length + 8, CHAR_COUNT + 40);
    CHECK(!with_table("cmap", cmap, length + 10 + 2 * CHAR_COUNT),
          "a format 6 map that counts more glyph ids than it holds is refused");
}

/*
 * One segment for the printable characters, read from the glyph id array,
 * and the 0xFFFF segment every format 4 map ends with.
 */
static int format4(unsigned char *subtable, const unsigned int *glyphs)
{
    const unsigned int segments = 2;
    int length = 16 + 8 * (int)segments + 2 * CHAR_COUNT;
    int i;

    memset(subtable, 0, (size_t)length);
    font_file_write16(subtable, 4);
    font_file_write16(subtable + 2, (unsigned int)length);
    font_file_write16(subtable + 6, 2 * segments);
    font_file_write16(subtable + 8, 4);
    font_file_write16(subtable + 10, 1);
    font_file_write16(subtable + 12, 0);
    font_file_write16(subtable + 14, FIRST_CHAR + CHAR_COUNT - 1);
    font_file_write16(subtable + 16, 0xffff);
    font_file_write16(subtable + 20, FIRST_CHAR);
    font_file_write16(subtable + 22, 0xffff);
    font_file_write16(subtable + 26, 1);
    font_file_write16(subtable + 28, 4);
    for (i = 0; i < CHAR_COUNT; i++) {
        font_file_write16(subtable + 32 + 2 * i, glyphs[i]);
    }

    return length;
}

static void check_format4(const unsigned int *glyphs)
{
    unsigned char cmap[512];
    int start = cmap_header(cmap, 3, 1);
    int length = start + format4(cmap + start, glyphs);

    CHECK(with_table("cmap", cmap, length), "a correct format 4 map is accepted and drawn");

    start = cmap_header(cmap, 3, 1);
    length = start + format4(cmap + start, glyphs);
    font_file_write16(cmap + start + 8, 0x4000);
    CHECK(!with_table("cmap", cmap, length),
          "a format 4 search range stb would index past the segments with is refused");

    start = cmap_header(cmap, 3, 1);
    length = start + format4(cmap + start, glyphs);
    font_file_write16(cmap + start + 28, 4 + 2 * 60);
    CHECK(!with_table("cmap", cmap, length),
          "a format 4 segment whose glyph ids run past the map is refused");

    start = cmap_header(cmap, 3, 1);
    length = start + format4(cmap + start, glyphs);
    font_file_write16(cmap + start + 6, 2 * 40);
    CHECK(!with_table("cmap", cmap, length),
          "a format 4 map that counts more segments than it holds is refused");
}

static void check_format12(const unsigned int *glyphs)
{
    unsigned char cmap[64];
    int start = cmap_header(cmap, 3, 10);
    int length = start + 28;

    memset(cmap + start, 0, 28);
    font_file_write16(cmap + start, 12);
    font_file_write32(cmap + start + 4, 28);
    font_file_write32(cmap + start + 12, 1);
    font_file_write32(cmap + start + 16, 'A');
    font_file_write32(cmap + start + 20, 'A');
    font_file_write32(cmap + start + 24, glyphs['A' - FIRST_CHAR]);
    CHECK(with_table("cmap", cmap, length), "a correct format 12 map is accepted and drawn");

    font_file_write32(cmap + start + 12, 0x10000000);
    CHECK(!with_table("cmap", cmap, length),
          "a format 12 map that counts more groups than it holds is refused");
}

/* stb uses the last Unicode record, so only that one has to be right. */
static void check_record_choice(const unsigned int *glyphs)
{
    unsigned char cmap[512];
    int good;
    int bad_at;

    memset(cmap, 0, 20);
    font_file_write16(cmap + 2, 2);
    font_file_write16(cmap + 4, 3);
    font_file_write16(cmap + 6, 1);
    font_file_write16(cmap + 12, 0);
    font_file_write16(cmap + 14, 3);

    good = format6(cmap + 20, glyphs, CHAR_COUNT);
    bad_at = 20 + good;
    font_file_write16(cmap + bad_at, 6);
    font_file_write16(cmap + bad_at + 2, 8);

    font_file_write32(cmap + 8, 20);
    font_file_write32(cmap + 16, (unsigned int)bad_at);
    CHECK(!with_table("cmap", cmap, bad_at + 8), "a damaged map in the record stb uses is refused");

    font_file_write32(cmap + 8, (unsigned int)bad_at);
    font_file_write32(cmap + 16, 20);
    CHECK(with_table("cmap", cmap, bad_at + 8), "a damaged map in a record stb never reads is accepted");

    cmap_header(cmap, 0, 5);
    font_file_write16(cmap + 12, 14);
    font_file_write32(cmap + 14, 10);
    CHECK(!with_table("cmap", cmap, 22), "a map format stb cannot read is refused");
}

/* Offset and length of a glyph in the glyph table of `file`. */
static void glyph_bytes(const FontFile *file, unsigned int index, int *start, int *length)
{
    int loca;
    int loca_length;
    int head;
    int head_length;
    int glyf;
    int glyf_length;
    int long_offsets;
    unsigned int first;
    unsigned int next;

    font_file_table(file, "loca", &loca, &loca_length);
    font_file_table(file, "head", &head, &head_length);
    font_file_table(file, "glyf", &glyf, &glyf_length);
    long_offsets = font_file_read16(file->data + head + 50) != 0;

    if (long_offsets) {
        const unsigned char *at = file->data + loca + 4 * index;

        first = (font_file_read16(at) << 16) | font_file_read16(at + 2);
        next = (font_file_read16(at + 4) << 16) | font_file_read16(at + 6);
    } else {
        first = font_file_read16(file->data + loca + 2 * index) * 2;
        next = font_file_read16(file->data + loca + 2 * index + 2) * 2;
    }

    *start = glyf + (int)first;
    *length = (int)(next - first);
}

/* Overwrites the start of a glyph's bytes and fixes the glyph table checksum. */
static int with_glyph(unsigned int index, const unsigned char *bytes, int length)
{
    FontFile edited = font_file_copy(&original);
    int start;
    int available;
    int loaded;

    glyph_bytes(&edited, index, &start, &available);
    if (length > available) {
        font_file_free(&edited);
        return -1;
    }

    memcpy(edited.data + start, bytes, (size_t)length);
    font_file_fix_checksum(&edited, "glyf");
    loaded = loads(&edited);
    font_file_free(&edited);
    return loaded;
}

static int composite(unsigned char *glyph, unsigned int part)
{
    memset(glyph, 0, 16);
    font_file_write16(glyph, 0xffff);
    font_file_write16(glyph + 10, 0x0002);
    font_file_write16(glyph + 12, part);
    return 16;
}

static void check_outlines(const unsigned int *glyphs)
{
    unsigned int a = glyphs['A' - FIRST_CHAR];
    unsigned char glyph[64];
    int start;
    int length;

    glyph_bytes(&original, a, &start, &length);
    memcpy(glyph, original.data + start, 12);
    font_file_write16(glyph + 10, 60000);
    CHECK(with_glyph(a, glyph, 12) == 0, "a glyph that claims more points than its bytes hold is refused");

    composite(glyph, a);
    CHECK(with_glyph(a, glyph, 16) == 0, "a composite glyph that contains itself is refused");

    composite(glyph, glyphs['B' - FIRST_CHAR]);
    CHECK(with_glyph(a, glyph, 16) == 1, "a composite glyph made of another glyph is accepted and drawn");

    composite(glyph, glyphs['B' - FIRST_CHAR]);
    font_file_write16(glyph + 10, 0x0000);
    CHECK(with_glyph(a, glyph, 16) == 0, "a composite part placed by point matching is refused");

    memset(glyph, 0, 15);
    font_file_write16(glyph, 1);
    glyph[14] = 0x30;
    CHECK(with_glyph(a, glyph, 15) == 0, "a contour of one off-curve point is refused");

    glyph[14] = 0x31;
    CHECK(with_glyph(a, glyph, 15) == 1, "a contour of one on-curve point is accepted");
}

static int glyph_count(const FontFile *file)
{
    int maxp;
    int length;

    font_file_table(file, "maxp", &maxp, &length);
    return (int)font_file_read16(file->data + maxp + 4);
}

/* Marks every glyph some composite glyph of the original font is built from. */
static void mark_parts(unsigned char *used, int glyphs)
{
    int index;

    for (index = 0; index < glyphs; index++) {
        int start;
        int length;
        const unsigned char *glyph;
        int at = 10;
        unsigned int flags;

        glyph_bytes(&original, (unsigned int)index, &start, &length);
        glyph = original.data + start;
        if (length < 10 || font_file_read16(glyph) < 0x8000) {
            continue;
        }

        do {
            flags = font_file_read16(glyph + at);
            used[font_file_read16(glyph + at + 2)] = 1;
            at += 4 + ((flags & 1) ? 4 : 2);
            at += (flags & 0x08) ? 2 : (flags & 0x40) ? 4 : (flags & 0x80) ? 8 : 0;
        } while ((flags & 0x20) && at + 4 <= length);
    }
}

/*
 * Simple glyphs of printable characters that no other glyph is built from,
 * so turning them into a chain changes the nesting of nothing else.
 */
static int free_glyphs(const unsigned int *glyphs, unsigned int *chain, int wanted)
{
    int count = glyph_count(&original);
    unsigned char *used = calloc(1, (size_t)count);
    int found = 0;
    int i;

    mark_parts(used, count);

    for (i = 0; i < CHAR_COUNT && found < wanted; i++) {
        int start;
        int length;
        unsigned int glyph = glyphs[i];
        int taken = 0;
        int k;

        for (k = 0; k < found; k++) {
            taken |= chain[k] == glyph;
        }

        glyph_bytes(&original, glyph, &start, &length);
        if (glyph != 0 && !used[glyph] && !taken && length >= 16
            && font_file_read16(original.data + start) < 0x8000) {
            chain[found++] = glyph;
        }
    }

    free(used);
    return found;
}

/* `count` composites that each consist of the next; the last of a simple glyph. */
static int nested(const unsigned int *chain, int count)
{
    FontFile edited = font_file_copy(&original);
    unsigned char glyph[16];
    int start;
    int length;
    int loaded;
    int i;

    for (i = 0; i < count; i++) {
        glyph_bytes(&edited, chain[i], &start, &length);
        memcpy(edited.data + start, glyph, (size_t)composite(glyph, chain[i + 1]));
    }

    font_file_fix_checksum(&edited, "glyf");
    loaded = loads(&edited);
    font_file_free(&edited);
    return loaded;
}

static void check_nesting(const unsigned int *glyphs)
{
    unsigned int chain[10];

    if (free_glyphs(glyphs, chain, 10) < 10) {
        CHECK(0, "the font has ten simple glyphs no other glyph is built from");
        return;
    }

    CHECK(nested(chain, 9) == 0, "composites nested nine deep are refused");
    CHECK(nested(chain, 8) == 1, "composites nested eight deep are accepted and drawn");
}

static unsigned int next_random(unsigned int *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

/* Random damage in the tables stb reads, with every checksum made correct again. */
static void check_fuzz(void)
{
    static const char *tags[] = { "cmap", "glyf", "loca", "hmtx", "hhea", "maxp", "head" };
    unsigned int state = 1112;
    int accepted = 0;
    int round;

    for (round = 0; round < FUZZ_ROUNDS; round++) {
        FontFile edited = font_file_copy(&original);
        const char *tag = tags[next_random(&state) % 7];
        int changes = 1 + (int)(next_random(&state) % 3);
        int offset;
        int length;
        int i;

        font_file_table(&edited, tag, &offset, &length);
        for (i = 0; i < changes; i++) {
            int at = offset + (int)(next_random(&state) % (unsigned int)length);

            edited.data[at] ^= (unsigned char)(1 + next_random(&state) % 255);
        }
        font_file_fix_checksum(&edited, tag);

        accepted += loads(&edited);
        font_file_free(&edited);
    }

    printf("  (%d of %d damaged fonts with correct checksums were safe to accept)\n",
           accepted, FUZZ_ROUNDS);
    CHECK(1, "no damaged font with correct checksums reads outside its buffers");
}

int main(void)
{
    unsigned int glyphs[CHAR_COUNT];

    mock_efi_init();
    image_alloc(&canvas, 640, 480);

    if (!font_file_load(&original, "regular.ttf")) {
        CHECK(0, "assets/fonts/regular.ttf can be read");
        return test_result();
    }

    CHECK(loads(&original), "the bundled font is accepted and drawn");
    original_glyphs(glyphs);

    check_format6(glyphs);
    check_format4(glyphs);
    check_format12(glyphs);
    check_record_choice(glyphs);
    check_outlines(glyphs);
    check_nesting(glyphs);
    check_fuzz();

    font_file_free(&original);
    return test_result();
}
