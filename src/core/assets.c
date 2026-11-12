#include "core/assets.h"

#include "aarambh/gfx/color.h"
#include "efi/fs.h"
#include "lib/str.h"

#define GRADIENT_TOP_SHADE    150
#define GRADIENT_BOTTOM_SHADE 30
#define SPARE_ACCENT          0x8a90a8

static void load_font(Font *font, const char *name)
{
    UINTN size = 0;
    unsigned char *data = fs_read(name, &size);

    if (!font_init(font, data, (int)size)) {
        mem_free(data);
        font->data = NULL;
    }
}

static void use_fallback(Font *font, const Font *fallback)
{
    if (!font->loaded && fallback->loaded) {
        *font = *fallback;
    }
}

/* One readable font is enough: every missing or damaged one borrows it. */
static void apply_font_fallbacks(FontSet *fonts)
{
    use_fallback(&fonts->regular, &fonts->bold);
    use_fallback(&fonts->regular, &fonts->light);
    use_fallback(&fonts->regular, &fonts->mono);

    use_fallback(&fonts->light, &fonts->regular);
    use_fallback(&fonts->bold, &fonts->regular);
    use_fallback(&fonts->mono, &fonts->bold);
}

void assets_load_fonts(FontSet *fonts, const Config *config)
{
    load_font(&fonts->light, config->font_light);
    load_font(&fonts->regular, config->font_regular);
    load_font(&fonts->bold, config->font_bold);
    load_font(&fonts->mono, config->font_mono);

    apply_font_fallbacks(fonts);
}

static int load_image(Image *image, const char *name)
{
    UINTN size;
    void *data;
    int decoded;

    if (str_is_empty(name)) {
        return 0;
    }

    data = fs_read(name, &size);
    if (data == NULL) {
        return 0;
    }

    decoded = image_decode(image, data, (int)size);
    mem_free(data);
    return decoded;
}

static int load_background(Entry *entry, const Image *screen)
{
    Image original;
    int covered;

    if (!load_image(&original, entry->background_name)) {
        return 0;
    }

    covered = image_cover(&entry->background, &original, screen->width, screen->height);
    image_free(&original);

    return covered;
}

static int make_gradient(Image *image, const Image *screen, UINT32 accent)
{
    UINT32 top = color_shade(accent, GRADIENT_TOP_SHADE);
    UINT32 bottom = color_shade(accent, GRADIENT_BOTTOM_SHADE);

    if (!image_alloc(image, screen->width, screen->height)) {
        return 0;
    }

    image_gradient(image, top, bottom);
    image_vignette(image);
    return 1;
}

static int make_background(Entry *entry, const Image *screen)
{
    if (load_background(entry, screen)) {
        image_vignette(&entry->background);
        return 1;
    }

    return make_gradient(&entry->background, screen, entry->accent);
}

/*
 * Themes draw every background at full screen size without checking it, so
 * each entry must end up with one. The spare is reserved before anything else
 * is loaded and shared by the entries that memory runs out for; it is given
 * back when no entry needed it.
 */
int assets_load_entries(EntryList *entries, const Image *screen)
{
    Image spare;
    int spare_used = 0;
    int i;

    if (!make_gradient(&spare, screen, SPARE_ACCENT)) {
        return 0;
    }

    for (i = 0; i < entries->count; i++) {
        Entry *entry = &entries->items[i];

        load_image(&entry->logo, entry->logo_name);

        if (!make_background(entry, screen)) {
            entry->background = spare;
            spare_used = 1;
        }
    }

    if (!spare_used) {
        image_free(&spare);
    }

    return 1;
}
