/*
 * <key>.kernels lines with values longer than aarambh.efi can hold. Nothing
 * may be cut short: a command line that loses root= still boots, somewhere.
 */

#include <string.h>

#include "core/kernels.h"
#include "lib/memory.h"
#include "boot/linux.h"
#include "support/efi_mock.h"
#include "support/test.h"

EFI_GUID LoadedImageProtocol;

static char list_text[16384];

void *fs_read(const char *name, UINTN *size)
{
    char *copy = mem_alloc(strlen(list_text) + 1);

    strcpy(copy, list_text);
    return copy;
}

int fs_size(const char *name, UINTN *size) { return 0; }
int fs_read_into(const char *name, UINT8 *buffer, UINTN size, FsProgressCallback progress) { return 0; }
void fs_make_path(CHAR16 *output, int capacity, const char *name) { output[0] = 0; }
EFI_STATUS image_loader_load(const CHAR16 *path, EFI_HANDLE *child) { return EFI_NOT_FOUND; }
EFI_STATUS initrd_register(void *data, UINTN size) { return EFI_SUCCESS; }
void initrd_unregister(void) {}
void video_release(void) {}

static void append_repeated(char *text, char c, int count)
{
    size_t length = strlen(text);

    memset(text + length, c, (size_t)count);
    text[length + (size_t)count] = '\0';
}

/* One list line: title|id|kernel|initrd|<padding of `fill` x>root=UUID=important */
static void make_list(int id_length, int kernel_length, const char *initrd, int fill)
{
    strcpy(list_text, "@oneshot grub\nLinux|");
    append_repeated(list_text, 'i', id_length);
    strcat(list_text, "|\\");
    append_repeated(list_text, 'k', kernel_length - 1);
    strcat(list_text, "|");
    strcat(list_text, initrd);
    strcat(list_text, "|");
    append_repeated(list_text, 'x', fill);
    strcat(list_text, " root=UUID=important\n");
}

static Entry load(void)
{
    Entry entry;

    memset(&entry, 0, sizeof(entry));
    strcpy(entry.key, "test");
    kernels_load(&entry);
    return entry;
}

static int ends_with_root(const char *cmdline)
{
    const char *root = " root=UUID=important";
    size_t length = strlen(cmdline);

    return length >= strlen(root) && strcmp(cmdline + length - strlen(root), root) == 0;
}

#define ROOT_LENGTH 20

int main(void)
{
    Entry entry;

    mock_efi_init();

    make_list(8, 8, "\\initrd.img", 870 - ROOT_LENGTH);
    entry = load();
    CHECK(entry.kernel_count == 1 && linux_can_boot_direct(&entry.kernels[0])
          && strlen(entry.kernels[0].cmdline) == 870 && ends_with_root(entry.kernels[0].cmdline),
          "a command line of 870 characters keeps its last argument");

    make_list(8, 8, "\\initrd.img", AARAMBH_CMDLINE_MAX - 1 - ROOT_LENGTH);
    entry = load();
    CHECK(linux_can_boot_direct(&entry.kernels[0]) && ends_with_root(entry.kernels[0].cmdline),
          "the longest command line that fits is kept whole");

    make_list(8, 8, "\\initrd.img", AARAMBH_CMDLINE_MAX - ROOT_LENGTH);
    entry = load();
    CHECK(entry.kernel_count == 1 && !linux_can_boot_direct(&entry.kernels[0])
          && entry.kernels[0].cmdline[0] == '\0' && entry.kernels[0].id[0] == 'i',
          "one character more: the kernel stays in the list without direct boot");

    make_list(8, (int)sizeof(entry.kernels[0].kernel), "\\initrd.img", 10);
    entry = load();
    CHECK(entry.kernel_count == 1 && !linux_can_boot_direct(&entry.kernels[0]),
          "a kernel path that does not fit: no direct boot");

    make_list(8, 8, "\\a,\\b,\\c,\\d", 10);
    entry = load();
    CHECK(linux_can_boot_direct(&entry.kernels[0]), "four initrd parts are accepted");

    make_list(8, 8, "\\a,\\b,\\c,\\d,\\e", 10);
    entry = load();
    CHECK(entry.kernel_count == 1 && !linux_can_boot_direct(&entry.kernels[0]),
          "five initrd parts: no direct boot");

    make_list((int)sizeof(entry.kernels[0].id) - 1, 8, "\\initrd.img", 10);
    entry = load();
    CHECK(entry.kernel_count == 1, "the longest id that fits is listed");

    make_list((int)sizeof(entry.kernels[0].id), 8, "\\initrd.img", 10);
    entry = load();
    CHECK(entry.kernel_count == 0, "an id that does not fit would pick another kernel: not listed");

    strcpy(list_text, "@oneshot systemd-boot\nLinux 6.12|arch.conf\n");
    entry = load();
    CHECK(entry.kernel_count == 1 && entry.oneshot == ONESHOT_SYSTEMD_BOOT
          && !linux_can_boot_direct(&entry.kernels[0]),
          "a line with only a title and an id starts through the OS's loader");

    return test_result();
}
