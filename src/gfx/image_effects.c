#include "gfx/pixel.h"
#include "lib/math.h"
#include "lib/memory.h"

#define VIGNETTE_TOP_END        0.16f
#define VIGNETTE_TOP_STRENGTH   0.35f
#define VIGNETTE_BOTTOM_START   0.55f
#define VIGNETTE_BOTTOM_STRENGTH 0.6f

static int vignette_amount(float position)
{
    float darkness = 0.0f;

    if (position < VIGNETTE_TOP_END) {
        darkness = (VIGNETTE_TOP_END - position) / VIGNETTE_TOP_END * VIGNETTE_TOP_STRENGTH;
    }

    if (position > VIGNETTE_BOTTOM_START) {
        float range = 1.0f - VIGNETTE_BOTTOM_START;

        darkness = (position - VIGNETTE_BOTTOM_START) / range * VIGNETTE_BOTTOM_STRENGTH;
    }

    return (int)(darkness * 256.0f);
}

void image_vignette(Image *image)
{
    int x;
    int y;

    for (y = 0; y < image->height; y++) {
        int amount = vignette_amount((float)y / image->height);
        UINT32 *row;

        if (amount == 0) {
            continue;
        }

        row = image->pixels + y * image->width;
        for (x = 0; x < image->width; x++) {
            row[x] = pixel_mix(row[x], 0, (UINT32)amount) | PIXEL_ALPHA_MASK;
        }
    }
}

void image_blend(Image *destination, const Image *from, const Image *to, int amount)
{
    int count = destination->width * destination->height;
    int i;

    if (image_is_empty(from) || image_is_empty(to)) {
        return;
    }

    for (i = 0; i < count; i++) {
        destination->pixels[i] = pixel_mix(from->pixels[i], to->pixels[i], (UINT32)amount);
    }
}

void image_darken(Image *image, int amount)
{
    int count = image->width * image->height;
    UINT32 factor = (UINT32)(256 - amount);
    int i;

    for (i = 0; i < count; i++) {
        image->pixels[i] = pixel_scale(image->pixels[i], factor);
    }
}

void image_gradient(Image *image, UINT32 top, UINT32 bottom)
{
    int x;
    int y;

    for (y = 0; y < image->height; y++) {
        UINT32 amount = (UINT32)(y * 256 / image->height);
        UINT32 color = pixel_mix(top, bottom, amount) | PIXEL_ALPHA_MASK;
        UINT32 *row = image->pixels + y * image->width;

        for (x = 0; x < image->width; x++) {
            row[x] = color;
        }
    }
}

static void blur_line(UINT32 *pixels, int count, int stride, int radius, UINT32 *scratch)
{
    UINT32 sum_r = 0;
    UINT32 sum_g = 0;
    UINT32 sum_b = 0;
    UINT32 sum_a = 0;
    int window = radius * 2 + 1;
    int i;

    for (i = 0; i < count; i++) {
        scratch[i] = pixels[i * stride];
    }

    for (i = -radius; i <= radius; i++) {
        UINT32 pixel = scratch[clamp_int(i, 0, count - 1)];

        sum_a += pixel >> 24;
        sum_r += (pixel >> 16) & 0xff;
        sum_g += (pixel >> 8) & 0xff;
        sum_b += pixel & 0xff;
    }

    for (i = 0; i < count; i++) {
        UINT32 added = scratch[clamp_int(i + radius + 1, 0, count - 1)];
        UINT32 removed = scratch[clamp_int(i - radius, 0, count - 1)];

        pixels[i * stride] = ((sum_a / window) << 24) | ((sum_r / window) << 16)
                           | ((sum_g / window) << 8) | (sum_b / window);

        sum_a += (added >> 24) - (removed >> 24);
        sum_r += ((added >> 16) & 0xff) - ((removed >> 16) & 0xff);
        sum_g += ((added >> 8) & 0xff) - ((removed >> 8) & 0xff);
        sum_b += (added & 0xff) - (removed & 0xff);
    }
}

void image_blur(Image *image, int radius)
{
    int longest = image->width;
    UINT32 *scratch;
    int pass;
    int i;

    if (radius < 1) {
        return;
    }

    if (image->height > longest) {
        longest = image->height;
    }

    scratch = mem_alloc(sizeof(UINT32) * (mem_size_t)longest);
    if (scratch == NULL) {
        return;
    }

    for (pass = 0; pass < 3; pass++) {
        for (i = 0; i < image->height; i++) {
            blur_line(image->pixels + i * image->width, image->width, 1, radius, scratch);
        }
        for (i = 0; i < image->width; i++) {
            blur_line(image->pixels + i, image->height, image->width, radius, scratch);
        }
    }

    mem_free(scratch);
}

UINT32 image_random(UINT32 *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state >> 8;
}

void image_noise(Image *image, int strength, UINT32 seed)
{
    int count = image->width * image->height;
    UINT32 state = seed;
    int i;

    for (i = 0; i < count; i++) {
        int delta = (int)(image_random(&state) % (UINT32)(strength * 2 + 1)) - strength;
        UINT32 pixel = image->pixels[i];
        int red = clamp_int((int)((pixel >> 16) & 0xff) + delta, 0, 255);
        int green = clamp_int((int)((pixel >> 8) & 0xff) + delta, 0, 255);
        int blue = clamp_int((int)(pixel & 0xff) + delta, 0, 255);

        image->pixels[i] = (pixel & PIXEL_ALPHA_MASK) | ((UINT32)red << 16)
                         | ((UINT32)green << 8) | (UINT32)blue;
    }
}

int image_glow_sprite(Image *image, int size, UINT32 color)
{
    float center = size / 2.0f;
    int x;
    int y;

    if (!image_alloc(image, size, size)) {
        return 0;
    }

    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            float dx = (x + 0.5f - center) / center;
            float dy = (y + 0.5f - center) / center;
            float falloff = 1.0f - (dx * dx + dy * dy);
            UINT32 alpha = 0;

            if (falloff > 0) {
                alpha = (UINT32)(falloff * falloff * falloff * 255.0f);
            }

            image->pixels[y * size + x] = (alpha << 24) | (color & 0xffffff);
        }
    }

    return 1;
}

static float corner_distance(int position, int size, float radius)
{
    float center = position + 0.5f;

    if (center < radius) {
        return radius - center;
    }
    if (center > size - radius) {
        return center - (size - radius);
    }
    return 0.0f;
}

void image_round_corners(Image *image, float radius)
{
    int x;
    int y;

    for (y = 0; y < image->height; y++) {
        float dy = corner_distance(y, image->height, radius);
        UINT32 *row = image->pixels + y * image->width;

        if (dy <= 0.0f) {
            continue;
        }

        for (x = 0; x < image->width; x++) {
            float dx = corner_distance(x, image->width, radius);
            float coverage;
            UINT32 alpha;

            if (dx <= 0.0f) {
                continue;
            }

            coverage = radius + 0.5f - (float)math_sqrt(dx * dx + dy * dy);
            if (coverage >= 1.0f) {
                continue;
            }
            if (coverage < 0.0f) {
                coverage = 0.0f;
            }

            alpha = (UINT32)((row[x] >> 24) * coverage);
            row[x] = (row[x] & 0xffffff) | (alpha << 24);
        }
    }
}