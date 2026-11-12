#include "stb_config.h"
#include "stb_image.h"

#include "gfx/pixel.h"
#include "lib/memory.h"

/* A failed allocation leaves an empty image, so nothing draws through NULL. */
int image_alloc(Image *image, int width, int height)
{
    image->width = 0;
    image->height = 0;
    image->pixels = NULL;

    if (width <= 0 || height <= 0) {
        return 0;
    }

    image->pixels = mem_alloc((mem_size_t)width * height * 4);
    if (image->pixels == NULL) {
        return 0;
    }

    image->width = width;
    image->height = height;
    return 1;
}

void image_free(Image *image)
{
    mem_free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}

int image_is_empty(const Image *image)
{
    return image->pixels == NULL;
}

int image_decode(Image *image, const void *data, int size)
{
    int width;
    int height;
    int channels;
    int count;
    int i;
    unsigned char *rgba;

    rgba = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (rgba == NULL) {
        return 0;
    }

    if (!image_alloc(image, width, height)) {
        stbi_image_free(rgba);
        return 0;
    }

    count = width * height;
    for (i = 0; i < count; i++) {
        unsigned char *source = rgba + i * 4;

        image->pixels[i] = ((UINT32)source[3] << 24)
                         | ((UINT32)source[0] << 16)
                         | ((UINT32)source[1] << 8)
                         | source[2];
    }

    stbi_image_free(rgba);
    return 1;
}

void image_copy(Image *destination, const Image *source)
{
    mem_size_t size = (mem_size_t)source->width * source->height * 4;

    mem_copy(destination->pixels, source->pixels, size);
}

void image_make_opaque(Image *image)
{
    int count = image->width * image->height;
    int i;

    for (i = 0; i < count; i++) {
        image->pixels[i] |= PIXEL_ALPHA_MASK;
    }
}

void image_clear(Image *image)
{
    mem_set(image->pixels, 0, (mem_size_t)image->width * image->height * 4);
}
