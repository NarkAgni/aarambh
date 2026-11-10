#include "boot/launcher.h"

#include "boot/image_loader.h"
#include "boot/linux.h"
#include "boot/next_boot.h"
#include "boot/power.h"

static void run_power_action(EntryAction action)
{
    switch (action) {
    case ENTRY_ACTION_FIRMWARE:
        power_reboot_to_firmware();
        break;
    case ENTRY_ACTION_REBOOT:
        power_reboot();
        break;
    case ENTRY_ACTION_SHUTDOWN:
        power_shutdown();
        break;
    case ENTRY_ACTION_EFI:
        break;
    }
}

static int kernel_choice(int kernel)
{
    if (kernel < 0) {
        return 0;
    }

    return kernel;
}

int launcher_boots_direct(const Entry *entry, int kernel)
{
    return entry->action == ENTRY_ACTION_EFI && entry_has_kernels(entry)
        && linux_can_boot_direct(&entry->kernels[kernel_choice(kernel)]);
}

EFI_STATUS launcher_run(const Entry *entry, int kernel, const char *cmdline,
                        const PlymouthOverlay *overlay, FsProgressCallback progress)
{
    run_power_action(entry->action);

    if (launcher_boots_direct(entry, kernel)) {
        EFI_STATUS status = linux_boot(&entry->kernels[kernel_choice(kernel)], cmdline, overlay,
                                       progress);

        if (cmdline != NULL) {
            return status;
        }
    }

    next_boot_write(entry, kernel);
    return image_loader_start(entry->device, entry->loader);
}