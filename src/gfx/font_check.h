#ifndef AARAMBH_GFX_FONT_CHECK_H
#define AARAMBH_GFX_FONT_CHECK_H

/*
 * 1 when the font at `offset` in `data` is intact: every table lies inside
 * `size` bytes and matches its checksum, the required tables are there and
 * the glyph offsets stay inside the glyph table. stb_truetype trusts all of
 * this, so a damaged file must be turned away before it.
 */
int font_check(const unsigned char *data, int size, int offset);

#endif
