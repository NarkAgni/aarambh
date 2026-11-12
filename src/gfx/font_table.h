#ifndef AARAMBH_GFX_FONT_TABLE_H
#define AARAMBH_GFX_FONT_TABLE_H

#include <efi.h>

/* One table of a TrueType file, already known to lie inside the file. */
typedef struct {
    const unsigned char *data;
    UINT32 length;
} FontTable;

static inline UINT32 font_read16(const unsigned char *bytes)
{
    return ((UINT32)bytes[0] << 8) | bytes[1];
}

static inline UINT32 font_read32(const unsigned char *bytes)
{
    return (font_read16(bytes) << 16) | font_read16(bytes + 2);
}

static inline int font_read_signed16(const unsigned char *bytes)
{
    UINT32 value = font_read16(bytes);

    return value >= 0x8000 ? (int)value - 0x10000 : (int)value;
}

/*
 * 1 when the character map stb_truetype will use is complete and every
 * lookup it can make stays inside it.
 */
int font_check_cmap(const FontTable *cmap);

/*
 * 1 when every glyph outline can be read without leaving its own bytes and
 * composite glyphs nest no deeper than stb_truetype can follow. The glyph
 * offsets in `loca` must already be checked.
 */
int font_check_glyphs(const FontTable *loca, const FontTable *glyf, UINT32 glyphs,
                      int long_offsets);

#endif
