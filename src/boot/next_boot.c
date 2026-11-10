#include <efi.h>
#include <efilib.h>

#include "boot/next_boot.h"

#include "efi/fs.h"
#include "lib/str.h"

#define LOADER_ENTRY_MAX 192

static EFI_GUID systemd_loader_guid = {
    0x4a67b082, 0x0a4c, 0x41cf, { 0xb6, 0xc7, 0x44, 0x0b, 0x29, 0xbb, 0x8c, 0x4f }
};

static void write_grub_choice(const Entry *entry, int kernel)
{
    char file_name[64];
    char grub_command[256];

    str_copy(file_name, "next-", sizeof(file_name));
    str_append(file_name, entry->key, sizeof(file_name));
    str_append(file_name, ".cfg", sizeof(file_name));

    grub_command[0] = '\0';
    if (kernel >= 0) {
        str_copy(grub_command, "set default=\"", sizeof(grub_command));
        str_append(grub_command, entry->kernels[kernel].id, sizeof(grub_command));
        str_append(grub_command, "\"\n", sizeof(grub_command));
    }

    fs_write(file_name, grub_command);
}

static void write_systemd_boot_choice(const Entry *entry, int kernel)
{
    CHAR16 value[LOADER_ENTRY_MAX];
    const char *id;
    UINT32 attributes = EFI_VARIABLE_NON_VOLATILE
                      | EFI_VARIABLE_BOOTSERVICE_ACCESS
                      | EFI_VARIABLE_RUNTIME_ACCESS;
    int length = 0;

    if (kernel < 0) {
        kernel = 0;
    }

    id = entry->kernels[kernel].id;
    while (id[length] != '\0' && length < LOADER_ENTRY_MAX - 1) {
        value[length] = (CHAR16)(UINT8)id[length];
        length++;
    }
    value[length] = 0;

    RT->SetVariable(L"LoaderEntryOneShot", &systemd_loader_guid, attributes,
                    (length + 1) * sizeof(CHAR16), value);
}

void next_boot_write(const Entry *entry, int kernel)
{
    if (str_is_empty(entry->key) || !entry_has_kernels(entry)) {
        return;
    }

    switch (entry->oneshot) {
    case ONESHOT_GRUB:
        write_grub_choice(entry, kernel);
        break;
    case ONESHOT_SYSTEMD_BOOT:
        write_systemd_boot_choice(entry, kernel);
        break;
    case ONESHOT_NONE:
        break;
    }
}
