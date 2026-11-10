#ifndef AARAMBH_EFI_FS_H
#define AARAMBH_EFI_FS_H

#include <efi.h>
#include <efilib.h>

#define FS_BASE_DIR L"\\EFI\\aarambh\\"
#define FS_PATH_MAX 300

typedef void (*FsProgressCallback)(void);

EFI_STATUS fs_init(EFI_HANDLE device);

void fs_make_path(CHAR16 *output, int capacity, const char *name);
int fs_exists(const CHAR16 *path);

typedef int (*FsDirectoryCallback)(const CHAR16 *name, int is_directory, void *context);

void *fs_read(const char *name, UINTN *size);
void *fs_read_path(const CHAR16 *path, UINTN *size);
int fs_size(const char *name, UINTN *size);
int fs_read_into(const char *name, UINT8 *buffer, UINTN size, FsProgressCallback progress);
int fs_write(const char *name, const char *data);
int fs_write_data(const char *name, const void *data, UINTN size);
int fs_make_directory(const char *name);

void fs_list_directory(const CHAR16 *path, FsDirectoryCallback callback, void *context);

/* Same calls on any volume, for loaders on a second ESP. NULL is ours. */
#define FS_MAX_VOLUMES 16

int fs_volumes(EFI_HANDLE *handles, int capacity);
int fs_exists_on(EFI_HANDLE device, const CHAR16 *path);
void *fs_read_path_on(EFI_HANDLE device, const CHAR16 *path, UINTN *size);
void fs_list_directory_on(EFI_HANDLE device, const CHAR16 *path,
                          FsDirectoryCallback callback, void *context);

#endif