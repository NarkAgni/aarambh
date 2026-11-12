#ifndef AARAMBH_LIB_STR_H
#define AARAMBH_LIB_STR_H

#include "lib/memory.h"

mem_size_t str_length(const char *text);
void str_copy(char *destination, const char *source, int capacity);
void str_append(char *destination, const char *source, int capacity);
void str_append_int(char *destination, int value, int capacity);
void str_to_upper(char *destination, const char *source, int capacity);
int str_equal(const char *left, const char *right);
int str_is_empty(const char *text);
int str_fits(const char *text, mem_size_t capacity);
int str_to_int(const char *text);
int str_parse_hex_color(const char *text, UINT32 *color);
int str_starts_with_nocase(const char *text, const char *prefix);

char *str_trim(char *text);
int str_split(char *text, char separator, char **parts, int max_parts);
char *str_next_line(char **cursor);

#endif
