#include "gfx/font_check.h"

#include "gfx/font_table.h"

#define HEADER_SIZE       12
#define RECORD_SIZE       16
#define HEAD_MIN_SIZE     54
#define HHEA_MIN_SIZE     36
#define MAXP_MIN_SIZE     6
#define LOCA_FORMAT_AT    50
#define GLYPH_COUNT_AT    4
#define METRIC_COUNT_AT   34
#define ASCENT_AT         4
#define DESCENT_AT        6
#define HEAD_ADJUSTMENT_AT 8

typedef FontTable Table;

#define read16 font_read16
#define read32 font_read32

static int same_tag(const unsigned char *bytes, const char *tag)
{
    return bytes[0] == (unsigned char)tag[0] && bytes[1] == (unsigned char)tag[1]
        && bytes[2] == (unsigned char)tag[2] && bytes[3] == (unsigned char)tag[3];
}

static UINT32 table_count(const unsigned char *data, UINT32 offset)
{
    return read16(data + offset + 4);
}

static const unsigned char *table_record(const unsigned char *data, UINT32 offset, UINT32 index)
{
    return data + offset + HEADER_SIZE + index * RECORD_SIZE;
}

/*
 * The sum of the table as big-endian 32-bit words, padded with zeros. The
 * file-wide adjustment stored in "head" is not part of that table's own sum.
 */
static UINT32 table_sum(const unsigned char *record, const unsigned char *table, UINT32 length)
{
    UINT32 sum = 0;
    UINT32 i;

    for (i = 0; i < length; i++) {
        sum += (UINT32)table[i] << (24 - 8 * (i & 3));
    }

    if (same_tag(record, "head") && length >= HEAD_ADJUSTMENT_AT + 4) {
        sum -= read32(table + HEAD_ADJUSTMENT_AT);
    }

    return sum;
}

/*
 * Every table must lie inside the file and add up to the checksum stored for
 * it: an interrupted copy, a damaged sector or a file that only looks like a
 * font fails here, before any of its values are used.
 */
static int check_directory(const unsigned char *data, UINT32 size, UINT32 offset)
{
    UINT32 count = table_count(data, offset);
    UINT32 i;

    if ((UINT64)offset + HEADER_SIZE + (UINT64)count * RECORD_SIZE > size) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        const unsigned char *record = table_record(data, offset, i);
        UINT32 start = read32(record + 8);
        UINT32 length = read32(record + 12);

        if ((UINT64)start + length > size) {
            return 0;
        }

        if (table_sum(record, data + start, length) != read32(record + 4)) {
            return 0;
        }
    }

    return 1;
}

static int find_table(const unsigned char *data, UINT32 offset, const char *tag, Table *table)
{
    UINT32 count = table_count(data, offset);
    UINT32 i;

    for (i = 0; i < count; i++) {
        const unsigned char *record = table_record(data, offset, i);

        if (same_tag(record, tag)) {
            table->data = data + read32(record + 8);
            table->length = read32(record + 12);
            return 1;
        }
    }

    return 0;
}

/*
 * stb divides the pixel size by ascent minus descent; zero or less would
 * give an infinite scale and glyph boxes that overflow.
 */
static int check_metrics(const Table *hhea, const Table *hmtx, UINT32 glyphs)
{
    UINT32 metrics = read16(hhea->data + METRIC_COUNT_AT);

    if (font_read_signed16(hhea->data + ASCENT_AT) - font_read_signed16(hhea->data + DESCENT_AT) <= 0) {
        return 0;
    }

    if (metrics == 0 || metrics > glyphs) {
        return 0;
    }

    return (UINT64)metrics * 4 + (UINT64)(glyphs - metrics) * 2 <= hmtx->length;
}

static UINT32 glyph_start(const Table *loca, UINT32 index, int long_format)
{
    if (long_format) {
        return read32(loca->data + index * 4);
    }
    return read16(loca->data + index * 2) * 2;
}

/* Glyph offsets must rise and stay inside the glyph table, then each glyph is read. */
static int check_outlines(const Table *loca, const Table *glyf, UINT32 glyphs, int long_format)
{
    UINT32 entry_size = long_format ? 4 : 2;
    UINT32 previous = 0;
    UINT32 i;

    if ((UINT64)(glyphs + 1) * entry_size > loca->length) {
        return 0;
    }

    for (i = 0; i <= glyphs; i++) {
        UINT32 start = glyph_start(loca, i, long_format);

        if (start < previous || start > glyf->length) {
            return 0;
        }
        previous = start;
    }

    return font_check_glyphs(loca, glyf, glyphs, long_format);
}

int font_check(const unsigned char *data, int size, int offset)
{
    UINT32 bytes = (UINT32)size;
    UINT32 base = (UINT32)offset;
    Table cmap;
    Table head;
    Table hhea;
    Table hmtx;
    Table maxp;
    Table loca;
    Table glyf;
    UINT32 glyphs;
    UINT32 loca_format;

    if (data == NULL || size < HEADER_SIZE || offset < 0 || offset > size - HEADER_SIZE) {
        return 0;
    }

    if (!check_directory(data, bytes, base)) {
        return 0;
    }

    if (!find_table(data, base, "cmap", &cmap) || !font_check_cmap(&cmap)) {
        return 0;
    }

    if (!find_table(data, base, "head", &head) || head.length < HEAD_MIN_SIZE
        || !find_table(data, base, "hhea", &hhea) || hhea.length < HHEA_MIN_SIZE
        || !find_table(data, base, "hmtx", &hmtx)) {
        return 0;
    }

    /* Only TrueType outlines: stb's CFF reader trusts its offsets too. */
    if (!find_table(data, base, "maxp", &maxp) || maxp.length < MAXP_MIN_SIZE
        || !find_table(data, base, "loca", &loca)
        || !find_table(data, base, "glyf", &glyf)) {
        return 0;
    }

    glyphs = read16(maxp.data + GLYPH_COUNT_AT);
    loca_format = read16(head.data + LOCA_FORMAT_AT);

    return loca_format <= 1 && check_metrics(&hhea, &hmtx, glyphs)
        && check_outlines(&loca, &glyf, glyphs, loca_format == 1);
}
