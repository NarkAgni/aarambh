#ifndef AARAMBH_GFX_FONT_CHECK_H
#define AARAMBH_GFX_FONT_CHECK_H

/*
 * 1 when the TrueType font at `offset` in `data` is safe to hand to
 * stb_truetype, which trusts every value in it: each table lies inside `size`
 * bytes and matches its checksum, the character map stb will use and every
 * glyph outline can be read without leaving their own bytes, and composite
 * glyphs nest without loops. Fonts with CFF outlines (.otf) are refused.
 *
 * Checksums only catch accidental damage; the structure checks are what keep
 * a file that was built wrong, with correct checksums, from being read past
 * its end.
 */
int font_check(const unsigned char *data, int size, int offset);

#endif
