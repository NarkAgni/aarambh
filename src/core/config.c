#include "core/config.h"

#include "core/autodetect.h"
#include "core/clock.h"
#include "efi/fs.h"
#include "lib/str.h"

static void config_set_defaults(Config *config)
{
    mem_set(config, 0, sizeof(*config));

    config->timeout_seconds = 10;
    config->autodetect = 1;
    config->allow_edit = 1;
    config->write_combining = 1;
    str_copy(config->theme, "accordion", sizeof(config->theme));
    str_copy(config->default_entry, "0", sizeof(config->default_entry));
    str_copy(config->font_light, "fonts/light.ttf", sizeof(config->font_light));
    str_copy(config->font_regular, "fonts/regular.ttf", sizeof(config->font_regular));
    str_copy(config->font_bold, "fonts/bold.ttf", sizeof(config->font_bold));
    str_copy(config->font_mono, "fonts/mono.ttf", sizeof(config->font_mono));
}

static char *split_key_value(char *line)
{
    char *value = line;

    while (*value != '\0' && *value != ' ' && *value != '\t') {
        value++;
    }

    if (*value != '\0') {
        *value = '\0';
        value++;
    }

    return str_trim(value);
}

static void apply_setting(Config *config, EntryList *entries, const char *key, char *value)
{
    if (str_equal(key, "timeout")) {
        config->timeout_seconds = str_to_int(value);
    } else if (str_equal(key, "default")) {
        str_copy(config->default_entry, value, sizeof(config->default_entry));
    } else if (str_equal(key, "utc_offset")) {
        config->utc_offset_minutes = clock_parse_offset(value);
    } else if (str_equal(key, "autodetect")) {
        config->autodetect = str_equal(value, "yes");
    } else if (str_equal(key, "edit")) {
        config->allow_edit = !str_equal(value, "no");
    } else if (str_equal(key, "write_combining")) {
        config->write_combining = !str_equal(value, "no");
    } else if (str_equal(key, "theme")) {
        str_copy(config->theme, value, sizeof(config->theme));
    } else if (str_equal(key, "font_light")) {
        str_copy(config->font_light, value, sizeof(config->font_light));
    } else if (str_equal(key, "font_regular")) {
        str_copy(config->font_regular, value, sizeof(config->font_regular));
    } else if (str_equal(key, "font_bold")) {
        str_copy(config->font_bold, value, sizeof(config->font_bold));
    } else if (str_equal(key, "font_mono")) {
        str_copy(config->font_mono, value, sizeof(config->font_mono));
    } else if (str_equal(key, "entry")) {
        entries_add_line(entries, value);
    }
}

static int is_number(const char *text)
{
    if (*text == '\0') {
        return 0;
    }

    while (*text != '\0') {
        if (*text < '0' || *text > '9') {
            return 0;
        }
        text++;
    }

    return 1;
}

static int same_name(const char *left, const char *right)
{
    return str_length(left) == str_length(right) && str_starts_with_nocase(left, right);
}

/*
 * "default" is an index, an entry key (ubuntu, fedora, windows) or a title.
 * Names are matched after autodetect, so they survive any change of order.
 */
static int find_default(const EntryList *entries, const char *name)
{
    int i;

    if (is_number(name)) {
        return str_to_int(name);
    }

    for (i = 0; i < entries->count; i++) {
        if (!str_is_empty(entries->items[i].key) && same_name(entries->items[i].key, name)) {
            return i;
        }
    }

    for (i = 0; i < entries->count; i++) {
        if (same_name(entries->items[i].title, name)) {
            return i;
        }
    }

    return 0;
}

static void parse_config(Config *config, EntryList *entries, char *text)
{
    char *cursor = text;
    char *line = str_next_line(&cursor);

    while (line != NULL) {
        if (line[0] != '\0' && line[0] != '#') {
            char *value = split_key_value(line);

            apply_setting(config, entries, line, value);
        }

        line = str_next_line(&cursor);
    }
}

void config_load(Config *config, EntryList *entries)
{
    char *text;

    config_set_defaults(config);

    text = fs_read(CONFIG_FILE_NAME, NULL);
    if (text != NULL) {
        parse_config(config, entries, text);
        mem_free(text);
    }

    if (entries->count == 0) {
        entries_add_actions(entries);
    }

    if (config->autodetect) {
        autodetect_entries(entries);
    }

    entries_move_actions_last(entries);

    config->default_index = find_default(entries, config->default_entry);
}