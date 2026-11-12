#include "lib/str.h"

static int is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

mem_size_t str_length(const char *text)
{
    mem_size_t length = 0;

    while (text[length] != '\0') {
        length++;
    }

    return length;
}

void str_copy(char *destination, const char *source, int capacity)
{
    int i = 0;

    while (source[i] != '\0' && i < capacity - 1) {
        destination[i] = source[i];
        i++;
    }

    destination[i] = '\0';
}

void str_append(char *destination, const char *source, int capacity)
{
    int length = (int)str_length(destination);

    str_copy(destination + length, source, capacity - length);
}

void str_append_int(char *destination, int value, int capacity)
{
    char digits[12];
    char text[13];
    int count = 0;
    int position = 0;
    unsigned int magnitude;

    if (value < 0) {
        text[position] = '-';
        position++;
        magnitude = (unsigned int)(-value);
    } else {
        magnitude = (unsigned int)value;
    }

    do {
        digits[count] = (char)('0' + magnitude % 10);
        count++;
        magnitude /= 10;
    } while (magnitude > 0);

    while (count > 0) {
        count--;
        text[position] = digits[count];
        position++;
    }

    text[position] = '\0';
    str_append(destination, text, capacity);
}

void str_to_upper(char *destination, const char *source, int capacity)
{
    int i = 0;

    while (source[i] != '\0' && i < capacity - 1) {
        char c = source[i];

        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }

        destination[i] = c;
        i++;
    }

    destination[i] = '\0';
}

int str_equal(const char *left, const char *right)
{
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }

    return *left == *right;
}

int str_is_empty(const char *text)
{
    return text[0] == '\0';
}

/* 1 when `text` and its terminator fit an array of `capacity` chars. */
int str_fits(const char *text, mem_size_t capacity)
{
    return str_length(text) < capacity;
}

#define INT_LIMIT 2147483647

/* Leading digits, saturated at the largest int instead of overflowing. */
int str_to_int(const char *text)
{
    int value = 0;
    int negative = 0;

    if (*text == '-') {
        negative = 1;
        text++;
    }

    while (is_digit(*text)) {
        int digit = *text - '0';

        if (value > (INT_LIMIT - digit) / 10) {
            value = INT_LIMIT;
        } else {
            value = value * 10 + digit;
        }
        text++;
    }

    if (negative) {
        return -value;
    }
    return value;
}

/*
 * 1 when `text` is only decimal digits; the value, capped at `max`, goes to
 * `value`. Any number of digits is safe: the cap is applied while reading.
 */
int str_parse_count(const char *text, int max, int *value)
{
    int result = 0;

    if (!is_digit(*text)) {
        return 0;
    }

    while (is_digit(*text)) {
        if (result <= max) {
            result = result * 10 + (*text - '0');
        }
        text++;
    }

    if (*text != '\0') {
        return 0;
    }

    *value = result > max ? max : result;
    return 1;
}

char *str_trim(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t') {
        text++;
    }

    end = text + str_length(text);
    while (end > text && is_blank(end[-1])) {
        end--;
        *end = '\0';
    }

    return text;
}

int str_split(char *text, char separator, char **parts, int max_parts)
{
    int count = 0;
    char *cursor;
    int i;

    parts[count] = text;
    count++;

    for (cursor = text; *cursor != '\0' && count < max_parts; cursor++) {
        if (*cursor == separator) {
            *cursor = '\0';
            parts[count] = cursor + 1;
            count++;
        }
    }

    for (i = 0; i < count; i++) {
        parts[i] = str_trim(parts[i]);
    }

    return count;
}

char *str_next_line(char **cursor)
{
    char *line = *cursor;
    char *end = line;

    if (*line == '\0') {
        return NULL;
    }

    while (*end != '\0' && *end != '\n') {
        end++;
    }

    if (*end != '\0') {
        *end = '\0';
        end++;
    }

    *cursor = end;
    return str_trim(line);
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int str_parse_hex_color(const char *text, UINT32 *color)
{
    UINT32 value = 0;
    int i;

    if (*text == '#') {
        text++;
    }

    for (i = 0; i < 6; i++) {
        int digit = hex_digit(text[i]);

        if (digit < 0) {
            return 0;
        }
        value = (value << 4) | (UINT32)digit;
    }

    if (text[6] != '\0') {
        return 0;
    }

    *color = value;
    return 1;
}

static char lower_ascii(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

int str_starts_with_nocase(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (lower_ascii(*text) != lower_ascii(*prefix)) {
            return 0;
        }
        text++;
        prefix++;
    }

    return 1;
}
