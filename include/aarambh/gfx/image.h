#ifndef AARAMBH_GFX_IMAGE_H
#define AARAMBH_GFX_IMAGE_H

#include <efi.h>

typedef struct {
    int width;
    int height;
    UINT32 *pixels;
} Image;

int image_alloc(Image *image, int width, int height);
void image_free(Image *image);
int image_is_empty(const Image *image);
int image_decode(Image *image, const void *data, int size);
void image_copy(Image *destination, const Image *source);
void image_make_opaque(Image *image);

int image_cover(Image *destination, const Image *source, int width, int height);

void image_vignette(Image *image);
void image_blend(Image *destination, const Image *from, const Image *to, int amount);
void image_darken(Image *image, int amount);
void image_gradient(Image *image, UINT32 top, UINT32 bottom);
void image_clear(Image *image);

/* Box blur, three passes, close to a Gaussian with the given radius. */
void image_blur(Image *image, int radius);

/* Makes the corners transparent, so the image draws as a rounded rectangle. */
void image_round_corners(Image *image, float radius);

/* Film grain: brightens or darkens pixels by up to `strength` (0..255). */
void image_noise(Image *image, int strength, UINT32 seed);

/* A soft round glow sprite of `color` with alpha falling off to the edge. */
int image_glow_sprite(Image *image, int size, UINT32 color);

UINT32 image_random(UINT32 *state);

#endif