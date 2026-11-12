#include "efi/fs.h"

#include "lib/memory.h"
#include "lib/str.h"

#define READ_CHUNK_SIZE 0x400000

static EFI_FILE_HANDLE root;

EFI_STATUS fs_init(EFI_HANDLE device)
{
    root = LibOpenRoot(device);
    if (root == NULL) {
        return EFI_LOAD_ERROR;
    }
    return EFI_SUCCESS;
}

static int is_absolute(const char *name)
{
    return name[0] == '\\' || name[0] == '/';
}

void fs_make_path(CHAR16 *output, int capacity, const char *name)
{
    int length = 0;

    if (!is_absolute(name)) {
        const CHAR16 *base = FS_BASE_DIR;

        while (*base != 0 && length < capacity - 1) {
            output[length] = *base;
            length++;
            base++;
        }
    }

    while (*name != '\0' && length < capacity - 1) {
        if (*name == '/') {
            output[length] = L'\\';
        } else {
            output[length] = (CHAR16)*name;
        }
        length++;
        name++;
    }

    output[length] = 0;
}

int fs_exists(const CHAR16 *path)
{
    EFI_FILE_HANDLE file;

    if (root->Open(root, &file, (CHAR16 *)path, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS) {
        return 0;
    }

    file->Close(file);
    return 1;
}

static EFI_FILE_HANDLE open_path(const CHAR16 *path, UINTN *size)
{
    EFI_FILE_HANDLE file;
    EFI_FILE_INFO *info;

    if (root->Open(root, &file, (CHAR16 *)path, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS) {
        return NULL;
    }

    info = LibFileInfo(file);
    if (info == NULL) {
        file->Close(file);
        return NULL;
    }

    *size = info->FileSize;
    FreePool(info);
    return file;
}

static EFI_FILE_HANDLE open_for_read(const char *name, UINTN *size)
{
    CHAR16 path[FS_PATH_MAX];

    fs_make_path(path, FS_PATH_MAX, name);
    return open_path(path, size);
}

static int read_chunks(EFI_FILE_HANDLE file, UINT8 *buffer, UINTN size)
{
    UINTN done = 0;

    while (done < size) {
        UINTN chunk = size - done;

        if (chunk > READ_CHUNK_SIZE) {
            chunk = READ_CHUNK_SIZE;
        }

        if (file->Read(file, &chunk, buffer + done) != EFI_SUCCESS || chunk == 0) {
            return 0;
        }

        done += chunk;
    }

    return 1;
}

static void *read_whole(EFI_FILE_HANDLE file, UINTN file_size, UINTN *size)
{
    UINT8 *buffer;

    if (file == NULL) {
        return NULL;
    }

    buffer = mem_alloc(file_size + 1);
    if (buffer != NULL && !read_chunks(file, buffer, file_size)) {
        mem_free(buffer);
        buffer = NULL;
    }

    file->Close(file);

    if (buffer == NULL) {
        return NULL;
    }

    buffer[file_size] = '\0';
    if (size != NULL) {
        *size = file_size;
    }

    return buffer;
}

void *fs_read(const char *name, UINTN *size)
{
    UINTN file_size = 0;
    EFI_FILE_HANDLE file = open_for_read(name, &file_size);

    return read_whole(file, file_size, size);
}

void *fs_read_path(const CHAR16 *path, UINTN *size)
{
    UINTN file_size = 0;
    EFI_FILE_HANDLE file = open_path(path, &file_size);

    return read_whole(file, file_size, size);
}

int fs_size(const char *name, UINTN *size)
{
    EFI_FILE_HANDLE file = open_for_read(name, size);

    if (file == NULL) {
        return 0;
    }

    file->Close(file);
    return 1;
}

int fs_read_into(const char *name, UINT8 *buffer, UINTN size, FsProgressCallback progress)
{
    EFI_FILE_HANDLE file;
    UINTN file_size;
    UINTN done = 0;
    int complete = 1;

    file = open_for_read(name, &file_size);
    if (file == NULL) {
        return 0;
    }

    if (file_size != size) {
        file->Close(file);
        return 0;
    }

    while (done < size) {
        UINTN chunk = size - done;

        if (chunk > READ_CHUNK_SIZE) {
            chunk = READ_CHUNK_SIZE;
        }

        if (file->Read(file, &chunk, buffer + done) != EFI_SUCCESS || chunk == 0) {
            complete = 0;
            break;
        }

        done += chunk;
        progress();
    }

    file->Close(file);
    return complete;
}

int fs_write(const char *name, const char *data)
{
    return fs_write_data(name, data, str_length(data));
}

int fs_make_directory(const char *name)
{
    CHAR16 path[FS_PATH_MAX];
    EFI_FILE_HANDLE file;
    UINT64 mode = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE;

    fs_make_path(path, FS_PATH_MAX, name);

    if (root->Open(root, &file, path, mode, EFI_FILE_DIRECTORY) != EFI_SUCCESS) {
        return 0;
    }

    file->Close(file);
    return 1;
}

/* Opening with CREATE keeps the old content of a file that is still there. */
static int truncate_file(EFI_FILE_HANDLE file)
{
    EFI_FILE_INFO *info = LibFileInfo(file);
    EFI_STATUS status = EFI_SUCCESS;

    if (info == NULL) {
        return 0;
    }

    if (info->FileSize != 0) {
        info->FileSize = 0;
        status = file->SetInfo(file, &GenericFileInfo, (UINTN)info->Size, info);
    }

    FreePool(info);
    return status == EFI_SUCCESS;
}

static int write_all(EFI_FILE_HANDLE file, const void *data, UINTN size)
{
    UINTN written = size;

    if (size == 0) {
        return 1;
    }

    return file->Write(file, &written, (void *)data) == EFI_SUCCESS && written == size;
}

/*
 * 1 only when the file now holds exactly `data`: a delete that fails, a short
 * write and a failed flush all leave old or partial content behind.
 */
int fs_write_data(const char *name, const void *data, UINTN size)
{
    CHAR16 path[FS_PATH_MAX];
    EFI_FILE_HANDLE file;
    UINT64 read_write = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE;
    int written;

    fs_make_path(path, FS_PATH_MAX, name);

    if (root->Open(root, &file, path, read_write, 0) == EFI_SUCCESS) {
        file->Delete(file);
    }

    if (root->Open(root, &file, path, read_write | EFI_FILE_MODE_CREATE, 0) != EFI_SUCCESS) {
        return 0;
    }

    written = truncate_file(file) && write_all(file, data, size);
    if (file->Flush(file) != EFI_SUCCESS) {
        written = 0;
    }

    file->Close(file);
    return written;
}

#define DIRECTORY_INFO_SIZE (SIZE_OF_EFI_FILE_INFO + 512)

void fs_list_directory(const CHAR16 *path, FsDirectoryCallback callback, void *context)
{
    EFI_FILE_HANDLE directory;
    UINT8 buffer[DIRECTORY_INFO_SIZE];

    if (root->Open(root, &directory, (CHAR16 *)path, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS) {
        return;
    }

    for (;;) {
        EFI_FILE_INFO *info = (EFI_FILE_INFO *)buffer;
        UINTN size = sizeof(buffer);
        int is_directory;

        if (directory->Read(directory, &size, buffer) != EFI_SUCCESS || size == 0) {
            break;
        }

        if (info->FileName[0] == L'.') {
            continue;
        }

        is_directory = (info->Attribute & EFI_FILE_DIRECTORY) != 0;
        if (!callback(info->FileName, is_directory, context)) {
            break;
        }
    }

    directory->Close(directory);
}