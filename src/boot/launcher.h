#ifndef AARAMBH_BOOT_LAUNCHER_H
#define AARAMBH_BOOT_LAUNCHER_H

#include "aarambh/entry.h"
#include "boot/plymouth_overlay.h"
#include "efi/fs.h"

/* 1 when Aarambh starts this kernel itself, so a Plymouth overlay reaches it. */
int launcher_boots_direct(const Entry *entry, int kernel);

/* Why launcher_run came back; it does not return when the system starts. */
typedef enum {
    LAUNCH_FAILED,
    LAUNCH_BLOCKED,
    LAUNCH_CHOICE_NOT_SAVED
} LaunchError;

/*
 * `cmdline` replaces the kernel's command line for this boot, or is NULL. An
 * edited boot never falls back to the OS's own loader, which would drop the edit.
 *
 * `kernel` is the index picked in the kernel list, or -1 for the default. A
 * picked kernel that cannot be handed to the OS's loader stops the launch,
 * because that loader would start a different kernel than the one chosen.
 */
LaunchError launcher_run(const Entry *entry, int kernel, const char *cmdline,
                         const PlymouthOverlay *overlay, FsProgressCallback progress);

#endif