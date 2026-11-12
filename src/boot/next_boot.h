#ifndef AARAMBH_BOOT_NEXT_BOOT_H
#define AARAMBH_BOOT_NEXT_BOOT_H

#include "aarambh/entry.h"

/*
 * Tells the OS's own loader which kernel to start next: `kernel` from the
 * list, or the default again for -1. An error means the loader still holds
 * whatever was stored before.
 */
EFI_STATUS next_boot_write(const Entry *entry, int kernel);

#endif
