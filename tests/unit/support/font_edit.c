#include "support/font_edit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int font_file_read16(const unsigned char *bytes)
{
    return ((unsigned int)bytes[0] << 8) | bytes[1];
}

static unsigned int read32(const unsigned char *bytes)
{
    return (font_file_read16(bytes) << 16) | font_file_read16(bytes + 2);
}

void font_file_write16(unsigned char *bytes, unsigned int value)
{
    bytes[0] = (unsigned char)(value >> 8);
    bytes[1] = (unsigned char)value;
}

void font_file_write32(unsigned char *bytes, unsigned int value)
{
    font_file_write16(bytes, value >> 16);
    font_file_write16(bytes + 2, value & 0xffff);
}

int font_file_load(FontFile *font, const char *name)
{
    char path[256];
    FILE *file;

    snprintf(path, sizeof(path), "assets/fonts/%s", name);
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    font->size = (int)ftell(file);
    fseek(file, 0, SEEK_SET);

    font->data = malloc((size_t)font->size);
    if (fread(font->data, 1, (size_t)font->size, file) != (size_t)font->size) {
        font->size = 0;
    }

    fclose(file);
    return font->size > 0;
}

void font_file_free(FontFile *font)
{
    free(font->data);
    font->data = NULL;
    font->size = 0;
}

FontFile font_file_copy(const FontFile *font)
{
    FontFile copy;

    copy.size = font->size;
    copy.data = malloc((size_t)font->size);
    memcpy(copy.data, font->data, (size_t)font->size);
    return copy;
}

static unsigned char *find_record(const FontFile *font, const char *tag)
{
    unsigned int count = font_file_read16(font->data + 4);
    unsigned int i;

    for (i = 0; i < count; i++) {
        unsigned char *record = font->data + 12 + 16 * i;

        if (memcmp(record, tag, 4) == 0) {
            return record;
        }
    }

    return NULL;
}

int font_file_table(const FontFile *font, const char *tag, int *offset, int *length)
{
    unsigned char *record = find_record(font, tag);

    if (record == NULL) {
        return 0;
    }

    *offset = (int)read32(record + 8);
    *length = (int)read32(record + 12);
    return 1;
}

static unsigned int table_sum(const char *tag, const unsigned char *table, unsigned int length)
{
    unsigned int sum = 0;
    unsigned int i;

    for (i = 0; i < length; i++) {
        sum += (unsigned int)table[i] << (24 - 8 * (i & 3));
    }

    if (memcmp(tag, "head", 4) == 0 && length >= 12) {
        sum -= read32(table + 8);
    }

    return sum;
}

void font_file_fix_checksum(FontFile *font, const char *tag)
{
    unsigned char *record = find_record(font, tag);
    unsigned int start = read32(record + 8);
    unsigned int length = read32(record + 12);

    font_file_write32(record + 4, table_sum(tag, font->data + start, length));
}

void font_file_replace_table(FontFile *font, const char *tag, const unsigned char *bytes,
                             int length)
{
    int start = (font->size + 3) & ~3;
    unsigned char *grown = calloc(1, (size_t)(start + length));
    unsigned char *record;

    memcpy(grown, font->data, (size_t)font->size);
    memcpy(grown + start, bytes, (size_t)length);
    free(font->data);
    font->data = grown;
    font->size = start + length;

    record = find_record(font, tag);
    font_file_write32(record + 8, (unsigned int)start);
    font_file_write32(record + 12, (unsigned int)length);
    font_file_fix_checksum(font, tag);
}
