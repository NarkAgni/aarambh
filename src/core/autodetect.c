#include "core/autodetect.h"

#include "efi/fs.h"
#include "efi/system.h"
#include "lib/str.h"

#define NAME_MAX 64

typedef struct {
    const char *directory;
    const char *title;
    const char *key;
} KnownLoader;

typedef struct {
    EntryList *list;
    EFI_HANDLE device;
} ScanContext;

static const KnownLoader known_loaders[] = {
    { "microsoft",   "Windows",       "windows" },
    { "ubuntu",      "Ubuntu",        "ubuntu" },
    { "debian",      "Debian",        "debian" },
    { "fedora",      "Fedora",        "fedora" },
    { "opensuse",    "openSUSE",      "opensuse" },
    { "arch",        "Arch Linux",    "arch" },
    { "manjaro",     "Manjaro",       "manjaro" },
    { "endeavouros", "EndeavourOS",   "endeavouros" },
    { "centos",      "CentOS",        "centos" },
    { "redhat",      "Red Hat",       "rhel" },
    { "rocky",       "Rocky Linux",   "rocky" },
    { "kali",        "Kali Linux",    "kali" },
    { "zorin",       "Zorin OS",      "zorin" },
    { "gentoo",      "Gentoo",        "gentoo" },
    { "void",        "Void Linux",    "void" },
    { "nixos",       "NixOS",         "nixos" },
    { "systemd",     "systemd-boot",  "systemd-boot" },
    { "refind",      "rEFInd",        "refind" },
    { "grub",        "Linux",         "linux" }
};

static const char *loader_candidates[] = {
    "shimx64.efi",
    "shim.efi",
    "grubx64.efi",
    "systemd-bootx64.efi",
    "refind_x64.efi",
    "Boot\\bootmgfw.efi"
};

static const char *skipped_directories[] = {
    "boot",
    "aarambh",
    "linux",
    "tools"
};

static void to_ascii(char *output, const CHAR16 *name, int capacity)
{
    int i = 0;

    while (name[i] != 0 && i < capacity - 1) {
        if (name[i] < 128) {
            output[i] = (char)name[i];
        } else {
            output[i] = '_';
        }
        i++;
    }

    output[i] = '\0';
}

static int names_match(const char *left, const char *right)
{
    return str_length(left) == str_length(right) && str_starts_with_nocase(left, right);
}

static int is_skipped(const char *directory)
{
    int count = (int)(sizeof(skipped_directories) / sizeof(skipped_directories[0]));
    int i;

    for (i = 0; i < count; i++) {
        if (names_match(directory, skipped_directories[i])) {
            return 1;
        }
    }

    return 0;
}

static const KnownLoader *find_known(const char *directory)
{
    int count = (int)(sizeof(known_loaders) / sizeof(known_loaders[0]));
    int i;

    for (i = 0; i < count; i++) {
        if (names_match(directory, known_loaders[i].directory)) {
            return &known_loaders[i];
        }
    }

    return NULL;
}

static int find_loader(EFI_HANDLE device, const char *directory, char *loader, int capacity)
{
    int count = (int)(sizeof(loader_candidates) / sizeof(loader_candidates[0]));
    int i;

    for (i = 0; i < count; i++) {
        CHAR16 path[FS_PATH_MAX];

        str_copy(loader, "\\EFI\\", capacity);
        str_append(loader, directory, capacity);
        str_append(loader, "\\", capacity);
        str_append(loader, loader_candidates[i], capacity);

        fs_make_path(path, FS_PATH_MAX, loader);
        if (fs_exists_on(device, path)) {
            return 1;
        }
    }

    return 0;
}

static void lowercase(char *output, const char *input, int capacity)
{
    int i = 0;

    while (input[i] != '\0' && i < capacity - 1) {
        char c = input[i];

        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        output[i] = c;
        i++;
    }

    output[i] = '\0';
}

static void add_detected(EntryList *list, EFI_HANDLE device, const char *directory,
                         const char *loader)
{
    const KnownLoader *known = find_known(directory);
    char title[NAME_MAX];
    char key[NAME_MAX];
    char logo[NAME_MAX + 16];
    char background[NAME_MAX + 24];
    char target[FS_PATH_MAX];
    char *fields[5];

    if (known != NULL) {
        str_copy(title, known->title, sizeof(title));
        str_copy(key, known->key, sizeof(key));
    } else {
        str_copy(title, directory, sizeof(title));
        lowercase(key, directory, sizeof(key));
    }

    str_copy(logo, "logos/", sizeof(logo));
    str_append(logo, key, sizeof(logo));
    str_append(logo, ".png", sizeof(logo));

    str_copy(background, "backgrounds/", sizeof(background));
    str_append(background, key, sizeof(background));
    str_append(background, ".jpg", sizeof(background));

    str_copy(target, loader, sizeof(target));

    fields[0] = title;
    fields[1] = target;
    fields[2] = logo;
    fields[3] = background;
    fields[4] = key;

    entries_add_on(list, device, fields, 5);

    if (list->count > 0) {
        list->items[list->count - 1].autodetected = 1;
    }
}

static int scan_directory(const CHAR16 *name, int is_directory, void *context)
{
    ScanContext *scan = context;
    char directory[NAME_MAX];
    char loader[FS_PATH_MAX];

    if (scan->list->count >= AARAMBH_MAX_ENTRIES) {
        return 0;
    }

    if (!is_directory) {
        return 1;
    }

    to_ascii(directory, name, sizeof(directory));

    if (is_skipped(directory) || entries_uses_directory(scan->list, directory)) {
        return 1;
    }

    if (find_loader(scan->device, directory, loader, sizeof(loader))) {
        add_detected(scan->list, scan->device, directory, loader);
    }

    return 1;
}

static void scan_volume(EntryList *list, EFI_HANDLE device)
{
    ScanContext context;

    context.list = list;
    context.device = device;
    fs_list_directory_on(device, L"\\EFI", scan_directory, &context);
}

void autodetect_entries(EntryList *list)
{
    EFI_HANDLE volumes[FS_MAX_VOLUMES];
    EFI_HANDLE own = system_device_handle();
    int count = fs_volumes(volumes, FS_MAX_VOLUMES);
    int i;

    scan_volume(list, NULL);

    for (i = 0; i < count; i++) {
        if (volumes[i] != own) {
            scan_volume(list, volumes[i]);
        }
    }
}