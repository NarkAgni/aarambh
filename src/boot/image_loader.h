#ifndef AARAMBH_BOOT_IMAGE_LOADER_H
#define AARAMBH_BOOT_IMAGE_LOADER_H

#include <efi.h>

EFI_STATUS image_loader_load(EFI_HANDLE device, const CHAR16 *path, EFI_HANDLE *child);
EFI_STATUS image_loader_start(EFI_HANDLE device, const CHAR16 *path);

#endif
