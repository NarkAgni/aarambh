/*
 * Numbers in aarambh.conf: any digits are safe, the timeout stays in range,
 * and anything that is not a number keeps the default.
 */

#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "core/navigation.h"
#include "lib/memory.h"
#include "lib/str.h"
#include "support/efi_mock.h"
#include "support/test.h"

static const char *config_text;

void *fs_read(const char *name, UINTN *size)
{
    char *copy = mem_alloc(strlen(config_text) + 1);

    strcpy(copy, config_text);
    return copy;
}

/* Two systems and Restart, as config_load would find them. */
void entries_add_line(EntryList *list, char *line) {}
void autodetect_entries(EntryList *list) {}
void entries_move_actions_last(EntryList *list) {}
int clock_parse_offset(const char *text) { return 0; }

void entries_add_actions(EntryList *list)
{
    list->count = 3;
    list->items[0].action = ENTRY_ACTION_EFI;
    strcpy(list->items[0].key, "ubuntu");
    list->items[1].action = ENTRY_ACTION_EFI;
    strcpy(list->items[1].key, "fedora");
    list->items[2].action = ENTRY_ACTION_REBOOT;
}

static Config load(const char *text)
{
    static EntryList entries;
    Config config;

    config_text = text;
    memset(&entries, 0, sizeof(entries));
    config_load(&config, &entries);
    return config;
}

static int timeout_of(const char *text)
{
    return load(text).timeout_seconds;
}

static void check_timeout(void)
{
    char long_number[256];

    memset(long_number, '9', 200);
    strcpy(long_number + 200, "\n");

    CHECK(timeout_of("") == 10, "no timeout line: the default of 10 seconds");
    CHECK(timeout_of("timeout 0\n") == 0, "timeout 0 turns the countdown off");
    CHECK(timeout_of("timeout 1\n") == 1, "timeout 1");
    CHECK(timeout_of("timeout 999\n") == 999, "timeout 999, the longest");
    CHECK(timeout_of("timeout 1000\n") == 999, "timeout 1000 counts as 999");
    CHECK(timeout_of("timeout 2147483647\n") == 999, "the largest int counts as 999");
    CHECK(timeout_of("timeout 2147483648\n") == 999, "the reported case: one past the largest int counts as 999");

    {
        char line[300];

        strcpy(line, "timeout ");
        strcat(line, long_number);
        CHECK(timeout_of(line) == 999, "200 digits count as 999");
    }

    CHECK(timeout_of("timeout -5\n") == 10, "a negative timeout keeps the default");
    CHECK(timeout_of("timeout 5s\n") == 10, "trailing letters keep the default");
    CHECK(timeout_of("timeout\n") == 10, "an empty value keeps the default");
    CHECK(timeout_of("timeout   7  \n") == 7, "spaces around the value are ignored");
}

static int selected_for(const char *text)
{
    static AppState app;
    Config config = load(text);
    static EntryList entries;

    memset(&app, 0, sizeof(app));
    memset(&entries, 0, sizeof(entries));
    entries_add_actions(&entries);
    app.config = config;
    app.entries = entries;
    nav_init(&app, 0.0f);
    return app.selected;
}

static void check_default(void)
{
    CHECK(selected_for("default 1\n") == 1, "default 1 selects the second system");
    CHECK(selected_for("default fedora\n") == 1, "default fedora selects it by key");
    CHECK(selected_for("default 99999999999999999999\n") == 0,
          "a default index of 20 digits is safe and starts at the first system");
    CHECK(selected_for("default 7\n") == 0, "a default index past the last entry starts at the first");
}

static void check_str_to_int(void)
{
    CHECK(str_to_int("42") == 42, "str_to_int reads a number");
    CHECK(str_to_int("99999999999") == 2147483647, "str_to_int saturates instead of overflowing");
    CHECK(str_to_int("-99999999999") == -2147483647, "and so does a negative one");
}

int main(void)
{
    mock_efi_init();

    check_timeout();
    check_default();
    check_str_to_int();

    return test_result();
}
