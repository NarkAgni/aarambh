#ifndef AARAMBH_ENTRY_H
#define AARAMBH_ENTRY_H

#include <efi.h>

#include "aarambh/gfx/image.h"

#define AARAMBH_MAX_ENTRIES 10
#define AARAMBH_MAX_KERNELS 16

typedef enum {
    ENTRY_ACTION_EFI,
    ENTRY_ACTION_FIRMWARE,
    ENTRY_ACTION_REBOOT,
    ENTRY_ACTION_SHUTDOWN
} EntryAction;

/* How a kernel picked from the list reaches the OS's own boot loader. */
typedef enum {
    ONESHOT_GRUB,
    ONESHOT_SYSTEMD_BOOT,
    ONESHOT_NONE
} OneShotMethod;

/*
 * One line of <key>.kernels, written by the OS-side hook. `initrd` may list
 * several ESP paths separated by commas; they are loaded back to back.
 */
typedef struct {
    char title[64];
    char id[192];
    char kernel[160];
    char initrd[384];
    char cmdline[768];
} Kernel;

/* One "entry" line of aarambh.conf, with its decoded logo and background. */
typedef struct {
    char title[64];
    char logo_name[128];
    char background_name[128];
    char key[32];
    CHAR16 loader[256];
    EFI_HANDLE device;
    EntryAction action;
    UINT32 accent;
    Image logo;
    Image background;
    Kernel *kernels;
    int kernel_count;
    OneShotMethod oneshot;
    int autodetected;
} Entry;

int entry_has_logo(const Entry *entry);
int entry_has_kernels(const Entry *entry);
int entry_is_system(const Entry *entry);

/* Text for the primary button: "Start", "Open", "Restart" or "Power off". */
const char *entry_action_label(const Entry *entry);

/* Line under the title, e.g. "Default system  -  Linux 6.8.0-45-generic". */
void entry_subtitle(const Entry *entry, int is_default, char *output, int capacity);

/* Splash heading, e.g. "Starting Ubuntu" or "Shutting down". */
void entry_launch_title(const Entry *entry, char *output, int capacity);

/* Kernel title shown under the splash heading, or "" when there is none. */
const char *entry_launch_detail(const Entry *entry, int kernel);

#endif
