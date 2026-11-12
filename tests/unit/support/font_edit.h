#ifndef AARAMBH_TESTS_UNIT_FONT_EDIT_H
#define AARAMBH_TESTS_UNIT_FONT_EDIT_H

/*
 * Builds damaged TrueType files whose checksums are still correct, so a test
 * reaches the structure checks behind the checksum check.
 */

typedef struct {
    unsigned char *data;
    int size;
} FontFile;

/* A copy of a bundled font from assets/fonts/, exactly as large as the file. */
int font_file_load(FontFile *font, const char *name);
void font_file_free(FontFile *font);
FontFile font_file_copy(const FontFile *font);

/* Offset and length of a table, or 0 when the font has no such table. */
int font_file_table(const FontFile *font, const char *tag, int *offset, int *length);

/* Recomputes the stored checksum of a table after its bytes were changed. */
void font_file_fix_checksum(FontFile *font, const char *tag);

/* Points a table at new bytes appended to the file, with a correct checksum. */
void font_file_replace_table(FontFile *font, const char *tag, const unsigned char *bytes,
                             int length);

unsigned int font_file_read16(const unsigned char *bytes);
void font_file_write16(unsigned char *bytes, unsigned int value);
void font_file_write32(unsigned char *bytes, unsigned int value);

#endif
