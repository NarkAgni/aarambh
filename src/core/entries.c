#include "core/entries.h"

#include "boot/power.h"
#include "core/kernels.h"
#include "efi/fs.h"
#include "lib/str.h"

#define LOADER_PATH_MAX 256
#define EFI_DIR_PREFIX  "\\EFI\\"

static const UINT32 accent_palette[] = {
    0xe95420,
    0x3c6eb4,
    0x8a90a8,
    0x2a9d8f,
    0xc0392b,
    0x7b5ea7,
    0xd4a017
};

static const char *action_entries[] = {
    "Firmware | @firmware | logos/settings.png | backgrounds/settings.jpg | | #8a90a8",
    "Restart | @reboot | logos/restart.png | backgrounds/restart.jpg | | #2a9d8f",
    "Power Off | @shutdown | logos/power.png | backgrounds/power.jpg | | #c0392b"
};

static const char *optional_field(char **fields, int field_count, int index)
{
    if (index < field_count) {
        return fields[index];
    }
    return "";
}

static UINT32 accent_for(int index, const char *custom)
{
    int palette_size = (int)(sizeof(accent_palette) / sizeof(accent_palette[0]));
    UINT32 color;

    if (str_parse_hex_color(custom, &color)) {
        return color;
    }

    return accent_palette[index % palette_size];
}

static int resolve_target(Entry *entry, const char *target)
{
    if (str_equal(target, "@firmware")) {
        entry->action = ENTRY_ACTION_FIRMWARE;
        return power_firmware_setup_supported();
    }

    if (str_equal(target, "@reboot")) {
        entry->action = ENTRY_ACTION_REBOOT;
        return 1;
    }

    if (str_equal(target, "@shutdown")) {
        entry->action = ENTRY_ACTION_SHUTDOWN;
        return 1;
    }

    entry->action = ENTRY_ACTION_EFI;
    fs_make_path(entry->loader, LOADER_PATH_MAX, target);

    if (!fs_exists_on(entry->device, entry->loader)) {
        return 0;
    }

    kernels_load(entry);
    return 1;
}

void entries_add_on(EntryList *list, EFI_HANDLE device, char **fields, int field_count)
{
    Entry *entry;

    if (list->count >= AARAMBH_MAX_ENTRIES || field_count < 2) {
        return;
    }

    if (str_is_empty(fields[0]) || str_is_empty(fields[1])) {
        return;
    }

    entry = &list->items[list->count];
    mem_set(entry, 0, sizeof(*entry));
    entry->device = device;

    str_copy(entry->title, fields[0], sizeof(entry->title));
    str_copy(entry->logo_name, optional_field(fields, field_count, 2), sizeof(entry->logo_name));
    str_copy(entry->background_name, optional_field(fields, field_count, 3),
             sizeof(entry->background_name));
    str_copy(entry->key, optional_field(fields, field_count, 4), sizeof(entry->key));
    entry->accent = accent_for(list->count, optional_field(fields, field_count, 5));
    entry->oneshot = ONESHOT_GRUB;

    if (resolve_target(entry, fields[1])) {
        list->count++;
    }
}

void entries_add(EntryList *list, char **fields, int field_count)
{
    entries_add_on(list, NULL, fields, field_count);
}

void entries_add_line(EntryList *list, char *line)
{
    char *fields[ENTRY_FIELD_COUNT];
    int field_count = str_split(line, '|', fields, ENTRY_FIELD_COUNT);

    entries_add(list, fields, field_count);
}

void entries_add_actions(EntryList *list)
{
    int count = (int)(sizeof(action_entries) / sizeof(action_entries[0]));
    int i;

    for (i = 0; i < count; i++) {
        char line[256];

        str_copy(line, action_entries[i], sizeof(line));
        entries_add_line(list, line);
    }
}

int entries_has_system(const EntryList *list)
{
    int i;

    for (i = 0; i < list->count; i++) {
        if (entry_is_system(&list->items[i])) {
            return 1;
        }
    }

    return 0;
}

static int loader_in_directory(const CHAR16 *loader, const char *directory)
{
    const char *prefix = EFI_DIR_PREFIX;
    int i = 0;
    int j = 0;

    while (prefix[j] != '\0') {
        if (loader[i] != (CHAR16)prefix[j]) {
            return 0;
        }
        i++;
        j++;
    }

    for (j = 0; directory[j] != '\0'; j++) {
        CHAR16 a = loader[i + j];
        char b = directory[j];

        if (a >= L'A' && a <= L'Z') {
            a = (CHAR16)(a - L'A' + L'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != (CHAR16)b) {
            return 0;
        }
    }

    return loader[i + j] == L'\\';
}

int entries_uses_directory(const EntryList *list, const char *directory)
{
    int i;

    for (i = 0; i < list->count; i++) {
        const Entry *entry = &list->items[i];

        if (entry_is_system(entry) && loader_in_directory(entry->loader, directory)) {
            return 1;
        }
    }

    return 0;
}

void entries_move_actions_last(EntryList *list)
{
    Entry sorted[AARAMBH_MAX_ENTRIES];
    int count = 0;
    int pass;
    int i;

    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < list->count; i++) {
            int is_system = entry_is_system(&list->items[i]);

            if ((pass == 0 && is_system) || (pass == 1 && !is_system)) {
                sorted[count] = list->items[i];
                count++;
            }
        }
    }

    mem_copy(list->items, sorted, sizeof(Entry) * (mem_size_t)count);
}
