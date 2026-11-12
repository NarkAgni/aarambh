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

static LaunchError launch_error(EFI_STATUS status)
{
    if (status == EFI_SECURITY_VIOLATION) {
        return LAUNCH_BLOCKED;
    }

    return LAUNCH_FAILED;
}

LaunchError launcher_run(const Entry *entry, int kernel, const char *cmdline,
                         const PlymouthOverlay *overlay, FsProgressCallback progress)
{
    EFI_STATUS status;

    run_power_action(entry->action);

    if (launcher_boots_direct(entry, kernel)) {
        status = linux_boot(&entry->kernels[kernel_choice(kernel)], cmdline, overlay, progress);

        if (cmdline != NULL) {
            return launch_error(status);
        }
    }

    status = next_boot_write(entry, kernel);
    if (EFI_ERROR(status) && kernel >= 0) {
        return LAUNCH_CHOICE_NOT_SAVED;
    }

    return launch_error(image_loader_start(entry->loader));
}