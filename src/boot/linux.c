#include <efi.h>
#include <efilib.h>

#include "boot/linux.h"

#include "boot/image_loader.h"
#include "boot/initrd.h"
#include "efi/video.h"
#include "lib/str.h"

#define MAX_INITRD_PARTS 4
#define INITRD_PATH_MAX  160

typedef struct {
    char paths[MAX_INITRD_PARTS][INITRD_PATH_MAX];
    UINTN sizes[MAX_INITRD_PARTS];
    int count;
    UINTN total;
} InitrdParts;

static CHAR16 load_options[sizeof(((Kernel *)0)->cmdline)];

static UINT32 build_load_options(const char *cmdline)
{
    int capacity = (int)(sizeof(load_options) / sizeof(load_options[0]));
    int length = 0;

    while (cmdline[length] != '\0' && length < capacity - 1) {
        load_options[length] = (CHAR16)(UINT8)cmdline[length];
        length++;
    }

    load_options[length] = 0;
    return (UINT32)((length + 1) * sizeof(CHAR16));
}

static void attach_cmdline(EFI_HANDLE child, const char *cmdline)
{
    EFI_LOADED_IMAGE *loaded;

    if (BS->HandleProtocol(child, &LoadedImageProtocol, (void **)&loaded) != EFI_SUCCESS) {
        return;
    }

    loaded->LoadOptionsSize = build_load_options(cmdline);
    loaded->LoadOptions = load_options;
}

static int collect_initrd_parts(InitrdParts *parts, const char *list)
{
    char buffer[sizeof(((Kernel *)0)->initrd)];
    char *fields[MAX_INITRD_PARTS];
    int field_count;
    int i;

    mem_set(parts, 0, sizeof(*parts));
    str_copy(buffer, list, sizeof(buffer));
    field_count = str_split(buffer, ',', fields, MAX_INITRD_PARTS);

    for (i = 0; i < field_count; i++) {
        char *path = parts->paths[parts->count];
        UINTN *size = &parts->sizes[parts->count];

        if (str_is_empty(fields[i])) {
            continue;
        }

        str_copy(path, fields[i], INITRD_PATH_MAX);
        if (!fs_size(path, size)) {
            return 0;
        }

        parts->total += *size;
        parts->count++;
    }

    return parts->count > 0;
}

static UINTN align4(UINTN value)
{
    return (value + 3) & ~(UINTN)3;
}

static void *load_initrd(const char *list, const PlymouthOverlay *overlay, UINTN *size,
                         FsProgressCallback progress)
{
    InitrdParts parts;
    UINT8 *buffer;
    UINTN offset = 0;
    UINTN total;
    int i;

    if (!collect_initrd_parts(&parts, list)) {
        return NULL;
    }

    total = parts.total;
    if (overlay->size > 0) {
        total = align4(parts.total) + overlay->size;
    }

    buffer = mem_alloc(total);
    if (buffer == NULL) {
        return NULL;
    }

    for (i = 0; i < parts.count; i++) {
        if (!fs_read_into(parts.paths[i], buffer + offset, parts.sizes[i], progress)) {
            mem_free(buffer);
            return NULL;
        }
        offset += parts.sizes[i];
    }

    if (overlay->size > 0) {
        mem_set(buffer + offset, 0, align4(offset) - offset);
        mem_copy(buffer + align4(offset), overlay->data, overlay->size);
    }

    *size = total;
    return buffer;
}

static EFI_STATUS register_initrd(const Kernel *kernel, const PlymouthOverlay *overlay,
                                  FsProgressCallback progress)
{
    UINTN size;
    void *data;

    if (str_is_empty(kernel->initrd)) {
        return EFI_SUCCESS;
    }

    data = load_initrd(kernel->initrd, overlay, &size, progress);
    if (data == NULL) {
        return EFI_NOT_FOUND;
    }

    return initrd_register(data, size);
}

int linux_can_boot_direct(const Kernel *kernel)
{
    return !str_is_empty(kernel->kernel);
}

EFI_STATUS linux_boot(const Kernel *kernel, const char *cmdline, const PlymouthOverlay *overlay,
                      FsProgressCallback progress)
{
    CHAR16 kernel_path[FS_PATH_MAX];
    EFI_HANDLE child;
    EFI_STATUS status;

    status = register_initrd(kernel, overlay, progress);
    if (EFI_ERROR(status)) {
        return status;
    }

    fs_make_path(kernel_path, FS_PATH_MAX, kernel->kernel);

    status = image_loader_load(NULL, kernel_path, &child);
    if (EFI_ERROR(status)) {
        initrd_unregister();
        return status;
    }

    attach_cmdline(child, cmdline != NULL ? cmdline : kernel->cmdline);

    video_release();
    status = BS->StartImage(child, NULL, NULL);
    initrd_unregister();
    return status;
}