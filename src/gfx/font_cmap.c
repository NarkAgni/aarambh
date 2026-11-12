#include "gfx/font_table.h"

#define CMAP_HEADER_SIZE   4
#define CMAP_RECORD_SIZE   8
#define PLATFORM_UNICODE   0
#define PLATFORM_MICROSOFT 3
#define MS_UNICODE_BMP     1
#define MS_UNICODE_FULL    10

#define FORMAT0_SIZE       6
#define FORMAT4_HEADER     16
#define FORMAT6_HEADER     10
#define FORMAT12_HEADER    16
#define FORMAT12_GROUP     12

static int stb_uses_record(UINT32 platform, UINT32 encoding)
{
    if (platform == PLATFORM_UNICODE) {
        return 1;
    }

    return platform == PLATFORM_MICROSOFT
        && (encoding == MS_UNICODE_BMP || encoding == MS_UNICODE_FULL);
}

/*
 * The subtable stb_truetype picks: the last record for Unicode, as in
 * stbtt_InitFont. 0 when there is none, which stb refuses as well.
 */
static UINT32 chosen_offset(const FontTable *cmap)
{
    UINT32 count = font_read16(cmap->data + 2);
    UINT32 offset = 0;
    UINT32 i;

    for (i = 0; i < count; i++) {
        const unsigned char *record = cmap->data + CMAP_HEADER_SIZE + i * CMAP_RECORD_SIZE;

        if (stb_uses_record(font_read16(record), font_read16(record + 2))) {
            offset = font_read32(record + 4);
        }
    }

    return offset;
}

static UINT32 floor_log2(UINT32 value)
{
    UINT32 result = 0;

    while (value > 1) {
        value >>= 1;
        result++;
    }

    return result;
}

/*
 * stb's binary search takes searchRange, entrySelector and rangeShift from
 * the file and indexes with them, so they must be the values the segment
 * count gives. A segment that reads glyph ids from the array must stay
 * inside the subtable for every character it covers.
 */
static int check_format4(const unsigned char *subtable, UINT32 available)
{
    UINT32 length;
    UINT32 segments;
    UINT32 search_range;
    UINT32 i;

    if (available < FORMAT4_HEADER) {
        return 0;
    }

    length = font_read16(subtable + 2);
    segments = font_read16(subtable + 6) / 2;

    if (length > available || segments == 0 || (font_read16(subtable + 6) & 1) != 0
        || FORMAT4_HEADER + 8 * segments > length) {
        return 0;
    }

    search_range = 2u << floor_log2(segments);
    if (font_read16(subtable + 8) != search_range
        || font_read16(subtable + 10) != floor_log2(segments)
        || font_read16(subtable + 12) != 2 * segments - search_range) {
        return 0;
    }

    for (i = 0; i < segments; i++) {
        UINT32 end = font_read16(subtable + 14 + 2 * i);
        UINT32 start = font_read16(subtable + 16 + 2 * segments + 2 * i);
        UINT32 range_at = 16 + 6 * segments + 2 * i;
        UINT32 range_offset = font_read16(subtable + range_at);

        if (i > 0 && end <= font_read16(subtable + 12 + 2 * i)) {
            return 0;
        }

        if (range_offset != 0 && start <= end
            && range_at + range_offset + 2 * (end - start) + 2 > length) {
            return 0;
        }
    }

    return 1;
}

static int check_format6(const unsigned char *subtable, UINT32 available)
{
    UINT32 length;

    if (available < FORMAT6_HEADER) {
        return 0;
    }

    length = font_read16(subtable + 2);
    return length <= available && length >= FORMAT6_HEADER
        && FORMAT6_HEADER + 2 * font_read16(subtable + 8) <= length;
}

static int check_format12(const unsigned char *subtable, UINT32 available)
{
    UINT32 length;

    if (available < FORMAT12_HEADER) {
        return 0;
    }

    length = font_read32(subtable + 4);
    return length <= available
        && FORMAT12_HEADER + (UINT64)FORMAT12_GROUP * font_read32(subtable + 12) <= length;
}

static int check_format0(const unsigned char *subtable, UINT32 available)
{
    UINT32 length;

    if (available < FORMAT0_SIZE) {
        return 0;
    }

    length = font_read16(subtable + 2);
    return length >= FORMAT0_SIZE && length <= available;
}

int font_check_cmap(const FontTable *cmap)
{
    UINT32 offset;
    const unsigned char *subtable;
    UINT32 available;

    if (cmap->length < CMAP_HEADER_SIZE
        || CMAP_HEADER_SIZE + (UINT64)font_read16(cmap->data + 2) * CMAP_RECORD_SIZE > cmap->length) {
        return 0;
    }

    offset = chosen_offset(cmap);
    if (offset == 0 || offset >= cmap->length || cmap->length - offset < 2) {
        return 0;
    }

    subtable = cmap->data + offset;
    available = cmap->length - offset;

    switch (font_read16(subtable)) {
    case 0:
        return check_format0(subtable, available);
    case 4:
        return check_format4(subtable, available);
    case 6:
        return check_format6(subtable, available);
    case 12:
    case 13:
        return check_format12(subtable, available);
    }

    return 0;
}
