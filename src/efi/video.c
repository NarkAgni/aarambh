#include <efi.h>
#include <efilib.h>

#include "efi/video.h"

#include "efi/fs.h"
#include "efi/wc.h"
#include "lib/str.h"

#define PREFERRED_MIN_WIDTH 1280
#define MAX_MODE_WIDTH 3840

/*
 * wc.state remembers whether write-combining works on this machine: "trying"
 * while the first attempt runs, "ok" once frames have been shown with it, and
 * "failed" when a boot finds "trying" left by an attempt that never got that
 * far. Deleting the file tries again.
 */
#define WC_STATE_FILE   "wc.state"
#define WC_CONFIRM_FRAMES 60

static EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
static UINT32 active_mode;
static int wc_wanted;
static int wc_confirm_frames;

static UINT32 mode_area(const EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info)
{
    return info->HorizontalResolution * info->VerticalResolution;
}

static void select_best_mode(void)
{
    UINT32 best_mode = gop->Mode->Mode;
    UINT32 best_area = mode_area(gop->Mode->Info);
    UINT32 mode;

    for (mode = 0; mode < gop->Mode->MaxMode; mode++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN info_size;
        UINT32 area;

        if (gop->QueryMode(gop, mode, &info_size, &info) != EFI_SUCCESS) {
            continue;
        }

        area = mode_area(info);
        if (area > best_area && info->HorizontalResolution <= MAX_MODE_WIDTH) {
            best_mode = mode;
            best_area = area;
        }
    }

    if (best_mode != gop->Mode->Mode) {
        gop->SetMode(gop, best_mode);
    }
}

EFI_STATUS video_init(Image *screen)
{
    if (LibLocateProtocol(&GraphicsOutputProtocol, (void **)&gop) != EFI_SUCCESS) {
        return EFI_UNSUPPORTED;
    }

    if (gop->Mode->Info->HorizontalResolution < PREFERRED_MIN_WIDTH) {
        select_best_mode();
    }

    active_mode = gop->Mode->Mode;

    if (!image_alloc(screen, gop->Mode->Info->HorizontalResolution,
                     gop->Mode->Info->VerticalResolution)) {
        return EFI_OUT_OF_RESOURCES;
    }

    return EFI_SUCCESS;
}

/* sfence drains the write-combining buffers, so the whole frame shows now. */
void video_present(const Image *screen)
{
    gop->Blt(gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)screen->pixels, EfiBltBufferToVideo,
             0, 0, 0, 0, screen->width, screen->height, 0);
    __asm__ volatile("sfence" : : : "memory");

    if (wc_confirm_frames > 0) {
        wc_confirm_frames--;
        if (wc_confirm_frames == 0) {
            fs_write(WC_STATE_FILE, "ok");
        }
    }
}

static int map_write_combining(void)
{
    if (!wc_wanted || gop->Mode->Info->PixelFormat == PixelBltOnly) {
        return 0;
    }

    return wc_enable(gop->Mode->FrameBufferBase, gop->Mode->FrameBufferSize);
}

void video_enable_write_combining(int allowed)
{
    char *state;

    if (!allowed) {
        return;
    }

    state = fs_read(WC_STATE_FILE, NULL);

    if (state == NULL) {
        if (fs_write(WC_STATE_FILE, "trying")) {
            wc_wanted = 1;
            if (map_write_combining()) {
                wc_confirm_frames = WC_CONFIRM_FRAMES;
            } else {
                fs_write(WC_STATE_FILE, "unsupported");
            }
        }
    } else if (str_equal(str_trim(state), "ok")) {
        wc_wanted = 1;
        map_write_combining();
    } else if (str_equal(str_trim(state), "trying")) {
        fs_write(WC_STATE_FILE, "failed");
    }

    mem_free(state);
}

/* Hand the firmware its original memory types back before another image runs. */
void video_release(void)
{
    wc_disable();
}

void video_clear(Image *screen)
{
    image_clear(screen);
    video_present(screen);
}

/* Back in the menu after a failed launch: the original mode, mapped fast again. */
void video_restore_mode(void)
{
    if (gop->Mode->Mode != active_mode) {
        gop->SetMode(gop, active_mode);
    }

    map_write_combining();
}
