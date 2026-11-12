#include "gfx/pixel.h"

static int image_halve(Image *image)
{
    Image half;
    int x;
    int y;

    if (!image_alloc(&half, image->width / 2, image->height / 2)) {
        return 0;
    }

    for (y = 0; y < half.height; y++) {
        const UINT32 *upper = image->pixels + (y * 2) * image->width;
        const UINT32 *lower = upper + image->width;

        for (x = 0; x < half.width; x++) {
            UINT32 top = pixel_lerp(upper[x * 2], upper[x * 2 + 1], 128);
            UINT32 bottom = pixel_lerp(lower[x * 2], lower[x * 2 + 1], 128);

            half.pixels[y * half.width + x] = pixel_lerp(top, bottom, 128);
        }
    }

    image_free(image);
    *image = half;
    return 1;
}

static int image_reduce(Image *working, const Image *source, int width, int height)
{
    int owned = 0;

    *working = *source;

    while (working->width >= width * 2 && working->height >= height * 2) {
        if (!owned) {
            Image copy;

            if (!image_alloc(&copy, working->width, working->height)) {
                break;
            }

            image_copy(&copy, working);
            *working = copy;
            owned = 1;
        }

        if (!image_halve(working)) {
            break;
        }
    }

    return owned;
}

static void image_resample(Image *destination, const Image *source)
{
    double scale_x = (double)source->width / destination->width;
    double scale_y = (double)source->height / destination->height;
    double scale = scale_x;
    double offset_x;
    double offset_y;
    int step;
    int start_x;
    int start_y;
    int x;
    int y;

    if (scale_y < scale) {
        scale = scale_y;
    }

    offset_x = (source->width - destination->width * scale) / 2.0;
    offset_y = (source->height - destination->height * scale) / 2.0;

    step = (int)(scale * 65536.0);
    start_x = (int)((offset_x + scale * 0.5 - 0.5) * 65536.0);
    start_y = (int)((offset_y + scale * 0.5 - 0.5) * 65536.0);

    for (y = 0; y < destination->height; y++) {
        int fixed_y = start_y + y * step;
        UINT32 *row = destination->pixels + y * destination->width;

        for (x = 0; x < destination->width; x++) {
            UINT32 pixel = image_sample(source, start_x + x * step, fixed_y);

            row[x] = pixel | PIXEL_ALPHA_MASK;
        }
    }
}

int image_cover(Image *destination, const Image *source, int width, int height)
{
    Image working;
    int owned;

    if (image_is_empty(source)) {
        return 0;
    }

    if (source->width == width && source->height == height) {
        if (!image_alloc(destination, width, height)) {
            return 0;
        }

        image_copy(destination, source);
        image_make_opaque(destination);
        return 1;
    }

    owned = image_reduce(&working, source, width, height);

    if (!image_alloc(destination, width, height)) {
        if (owned) {
            image_free(&working);
        }
        return 0;
    }

    image_resample(destination, &working);

    if (owned) {
        image_free(&working);
    }

    return 1;
}
