/*
 * A kernel picked from the list reaches the OS's own loader through a file or
 * a firmware variable. When that cannot be stored, the loader must not start:
 * it would boot a different kernel than the one chosen.
 */

#include <string.h>

#include "boot/launcher.h"
#include "lib/memory.h"
#include "support/efi_mock.h"
#include "support/test.h"

static Kernel kernels[2];
static Entry entry;

static char stored[256];
static int stored_exists;
static int write_fails;
static int writes;
static int loader_starts;
static int direct_boots;
static EFI_STATUS direct_status;

void *fs_read(const char *name, UINTN *size)
{
    char *copy;

    if (!stored_exists) {
        return NULL;
    }

    copy = mem_alloc(strlen(stored) + 1);
    strcpy(copy, stored);
    return copy;
}

int fs_write(const char *name, const char *data)
{
    writes++;

    if (write_fails) {
        return 0;
    }

    strcpy(stored, data);
    stored_exists = 1;
    return 1;
}

EFI_STATUS image_loader_start(const CHAR16 *path)
{
    loader_starts++;
    return EFI_LOAD_ERROR;
}

int linux_can_boot_direct(const Kernel *kernel)
{
    return kernel->kernel[0] != '\0';
}

EFI_STATUS linux_boot(const Kernel *kernel, const char *cmdline, const PlymouthOverlay *overlay,
                      FsProgressCallback progress)
{
    direct_boots++;
    return direct_status;
}

void power_reboot_to_firmware(void) {}
void power_reboot(void) {}
void power_shutdown(void) {}

static void progress(void) {}

static void reset(OneShotMethod method, const char *stored_choice)
{
    memset(kernels, 0, sizeof(kernels));
    strcpy(kernels[0].id, "newest");
    strcpy(kernels[1].id, "old-working-kernel");

    memset(&entry, 0, sizeof(entry));
    strcpy(entry.key, "test");
    entry.action = ENTRY_ACTION_EFI;
    entry.kernels = kernels;
    entry.kernel_count = 2;
    entry.oneshot = method;

    stored_exists = stored_choice != NULL;
    strcpy(stored, stored_choice != NULL ? stored_choice : "");
    write_fails = 0;
    writes = 0;
    loader_starts = 0;
    direct_boots = 0;
    direct_status = EFI_LOAD_ERROR;
    mock_set_variable_status = EFI_SUCCESS;
    mock_set_variable_calls = 0;
}

static LaunchError launch(int kernel, const char *cmdline)
{
    PlymouthOverlay overlay = { NULL, 0 };

    return launcher_run(&entry, kernel, cmdline, &overlay, progress);
}

static void check_grub(void)
{
    LaunchError error;

    reset(ONESHOT_GRUB, "");
    error = launch(1, NULL);
    CHECK(error == LAUNCH_FAILED && loader_starts == 1
          && strcmp(stored, "set default=\"old-working-kernel\"\n") == 0,
          "GRUB: a picked kernel is stored, then the loader starts");

    reset(ONESHOT_GRUB, "");
    write_fails = 1;
    error = launch(1, NULL);
    CHECK(error == LAUNCH_CHOICE_NOT_SAVED && loader_starts == 0,
          "GRUB: a picked kernel that cannot be stored stops the launch");

    reset(ONESHOT_GRUB, "");
    write_fails = 1;
    error = launch(-1, NULL);
    CHECK(error == LAUNCH_FAILED && writes == 0 && loader_starts == 1,
          "GRUB: the default with nothing stored needs no write at all");

    reset(ONESHOT_GRUB, NULL);
    error = launch(-1, NULL);
    CHECK(writes == 0 && loader_starts == 1, "GRUB: a missing file counts as no choice");

    reset(ONESHOT_GRUB, "set default=\"old-working-kernel\"\n");
    error = launch(-1, NULL);
    CHECK(writes == 1 && stored[0] == '\0' && loader_starts == 1,
          "GRUB: the default clears an older choice");

    reset(ONESHOT_GRUB, "set default=\"old-working-kernel\"\n");
    write_fails = 1;
    error = launch(-1, NULL);
    CHECK(error == LAUNCH_FAILED && loader_starts == 1,
          "GRUB: the default still starts when an older choice cannot be cleared");
}

static void check_systemd_boot(void)
{
    LaunchError error;

    reset(ONESHOT_SYSTEMD_BOOT, NULL);
    error = launch(1, NULL);
    CHECK(mock_set_variable_calls == 1 && loader_starts == 1,
          "systemd-boot: a picked kernel is stored, then the loader starts");

    reset(ONESHOT_SYSTEMD_BOOT, NULL);
    mock_set_variable_status = EFI_WRITE_PROTECTED;
    error = launch(1, NULL);
    CHECK(error == LAUNCH_CHOICE_NOT_SAVED && loader_starts == 0,
          "systemd-boot: a write-protected variable stops the launch");

    reset(ONESHOT_SYSTEMD_BOOT, NULL);
    mock_set_variable_status = EFI_OUT_OF_RESOURCES;
    error = launch(1, NULL);
    CHECK(error == LAUNCH_CHOICE_NOT_SAVED && loader_starts == 0,
          "systemd-boot: full variable storage stops the launch");

    reset(ONESHOT_SYSTEMD_BOOT, NULL);
    mock_set_variable_status = EFI_OUT_OF_RESOURCES;
    error = launch(-1, NULL);
    CHECK(error == LAUNCH_FAILED && loader_starts == 1,
          "systemd-boot: the default still starts, the loader picks its own default");
}

static void check_direct_boot(void)
{
    LaunchError error;

    reset(ONESHOT_GRUB, "");
    strcpy(kernels[0].kernel, "\\vmlinuz");
    direct_status = EFI_SECURITY_VIOLATION;
    error = launch(-1, "init=/bin/bash");
    CHECK(error == LAUNCH_BLOCKED && direct_boots == 1 && loader_starts == 0,
          "an edited command line never falls back to the loader");

    reset(ONESHOT_GRUB, "");
    strcpy(kernels[0].kernel, "\\vmlinuz");
    error = launch(-1, NULL);
    CHECK(direct_boots == 1 && loader_starts == 1,
          "a failed direct boot falls back to the loader");
}

int main(void)
{
    mock_efi_init();

    check_grub();
    check_systemd_boot();
    check_direct_boot();

    return test_result();
}
