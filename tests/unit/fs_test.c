/*
 * fs_write_data against a file system that misbehaves: the caller may only be
 * told "written" when the file holds exactly the new content.
 */

#include <stdlib.h>
#include <string.h>

#include "efi/fs.h"
#include "support/efi_mock.h"
#include "support/test.h"

EFI_GUID gEfiFileInfoGuid;

/* One file on a mocked volume. */
static char content[64];
static UINTN content_size;
static int file_exists;

static int delete_fails;
static int create_fails;
static int truncate_fails;
static int flush_fails;
static int write_fails;
static UINTN write_limit;

static struct _EFI_FILE_HANDLE root_handle;
static struct _EFI_FILE_HANDLE file_handle;

static EFI_STATUS EFIAPI mock_open(struct _EFI_FILE_HANDLE *self, struct _EFI_FILE_HANDLE **opened,
                                   CHAR16 *name, UINT64 mode, UINT64 attributes)
{
    if (!file_exists) {
        if ((mode & EFI_FILE_MODE_CREATE) == 0) {
            return EFI_NOT_FOUND;
        }
        if (create_fails) {
            return EFI_WRITE_PROTECTED;
        }

        file_exists = 1;
        content_size = 0;
    }

    *opened = &file_handle;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI mock_close(struct _EFI_FILE_HANDLE *self)
{
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI mock_delete(struct _EFI_FILE_HANDLE *self)
{
    if (delete_fails) {
        return EFI_WARN_DELETE_FAILURE;
    }

    file_exists = 0;
    content_size = 0;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI mock_write(struct _EFI_FILE_HANDLE *self, UINTN *size, VOID *data)
{
    if (write_fails) {
        return EFI_VOLUME_FULL;
    }

    if (write_limit > 0 && *size > write_limit) {
        *size = write_limit;
    }

    memcpy(content, data, *size);
    if (*size > content_size) {
        content_size = *size;
    }

    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI mock_set_info(struct _EFI_FILE_HANDLE *self, EFI_GUID *type, UINTN size,
                                       VOID *data)
{
    if (truncate_fails) {
        return EFI_WRITE_PROTECTED;
    }

    content_size = (UINTN)((EFI_FILE_INFO *)data)->FileSize;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI mock_flush(struct _EFI_FILE_HANDLE *self)
{
    return flush_fails ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}

EFI_FILE_HANDLE LibOpenRoot(EFI_HANDLE device)
{
    return &root_handle;
}

EFI_FILE_INFO *LibFileInfo(EFI_FILE_HANDLE handle)
{
    EFI_FILE_INFO *info = calloc(1, SIZE_OF_EFI_FILE_INFO + 64);

    info->Size = SIZE_OF_EFI_FILE_INFO + 64;
    info->FileSize = content_size;
    return info;
}

VOID FreePool(VOID *buffer)
{
    free(buffer);
}

static void reset(const char *old_content)
{
    file_exists = old_content != NULL;
    content_size = 0;

    if (old_content != NULL) {
        content_size = strlen(old_content);
        memcpy(content, old_content, content_size);
    }

    delete_fails = 0;
    create_fails = 0;
    truncate_fails = 0;
    flush_fails = 0;
    write_fails = 0;
    write_limit = 0;
}

static int holds(const char *expected)
{
    return file_exists && content_size == strlen(expected)
        && memcmp(content, expected, content_size) == 0;
}

int main(void)
{
    mock_efi_init();

    root_handle.Open = mock_open;
    file_handle.Open = mock_open;
    file_handle.Close = mock_close;
    file_handle.Delete = mock_delete;
    file_handle.Write = mock_write;
    file_handle.SetInfo = mock_set_info;
    file_handle.Flush = mock_flush;
    fs_init(NULL);

    reset(NULL);
    CHECK(fs_write("next-test.cfg", "set default=new\n") && holds("set default=new\n"),
          "a new file is written");

    reset("set default=old-kernel\n");
    CHECK(fs_write("next-test.cfg", "short\n") && holds("short\n"),
          "a longer old file is replaced completely");

    reset("set default=old-kernel\n");
    delete_fails = 1;
    CHECK(fs_write("next-test.cfg", "") && holds(""),
          "the delete fails: the old content is still cut away");

    reset("set default=old-kernel\n");
    delete_fails = 1;
    CHECK(fs_write("next-test.cfg", "short\n") && holds("short\n"),
          "the delete fails: no tail of the old content stays behind");

    reset("set default=old-kernel\n");
    delete_fails = 1;
    truncate_fails = 1;
    CHECK(!fs_write("next-test.cfg", ""),
          "the delete and the truncation fail: reported, the old content is still there");

    reset(NULL);
    create_fails = 1;
    CHECK(!fs_write("next-test.cfg", "set default=new\n"), "a file that cannot be created is reported");

    reset(NULL);
    write_fails = 1;
    CHECK(!fs_write("next-test.cfg", "set default=new\n"), "a failed write is reported");

    reset(NULL);
    write_limit = 1;
    CHECK(!fs_write("next-test.cfg", "failed"), "a write that stops after one byte is reported");

    reset(NULL);
    flush_fails = 1;
    CHECK(!fs_write("next-test.cfg", "set default=new\n"), "a failed flush is reported");

    return test_result();
}
