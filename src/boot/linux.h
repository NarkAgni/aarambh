#ifndef AARAMBH_BOOT_LINUX_H
#define AARAMBH_BOOT_LINUX_H

#include "aarambh/entry.h"
#include "boot/plymouth_overlay.h"
#include "efi/fs.h"

int linux_can_boot_direct(const Kernel *kernel);

/* 1 when an initrd list has few and short enough paths to be loaded whole. */
int linux_initrd_list_fits(const char *list);

/*
 * `cmdline` replaces the kernel's own command line when it is not NULL.
 * `overlay` is appended after the kernel's initrd; it may be empty.
 */
EFI_STATUS linux_boot(const Kernel *kernel, const char *cmdline, const PlymouthOverlay *overlay,
                      FsProgressCallback progress);

#endif