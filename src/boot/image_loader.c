#include <efi.h>
#include <efilib.h>

#include "boot/image_loader.h"

#include "boot/secure_boot.h"
#include "efi/fs.h"
#include "efi/system.h"
#include "efi/video.h"
#include "lib/memory.h"

EFI_STATUS image_loader_load(EFI_HANDLE device, const CHAR16 *path, EFI_HANDLE *child)
{
    EFI_DEVICE_PATH *device_path;
    EFI_STATUS status;
    void *buffer;
    UINTN size;

    *child = NULL;

    buffer = fs_read_path_on(device, path, &size);
    if (buffer == NULL) {
        return EFI_NOT_FOUND;
    }

    device_path = FileDevicePath(device != NULL ? device : system_device_handle(),
                                 (CHAR16 *)path);
    if (device_path == NULL) {
        mem_free(buffer);
        return EFI_NOT_FOUND;
    }

    secure_boot_override(buffer, size);
    status = BS->LoadImage(FALSE, system_image_handle(), device_path, buffer, size, child);
    secure_boot_restore();

    FreePool(device_path);
    mem_free(buffer);

    return status;
}

EFI_STATUS image_loader_start(EFI_HANDLE device, const CHAR16 *path)
{
    EFI_HANDLE child;
    EFI_STATUS status;

    status = image_loader_load(device, path, &child);
    if (EFI_ERROR(status)) {
        return status;
    }

    video_release();
    return BS->StartImage(child, NULL, NULL);
}