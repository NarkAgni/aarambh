#ifndef AARAMBH_CORE_ASSETS_H
#define AARAMBH_CORE_ASSETS_H

#include "aarambh/gfx/font.h"
#include "core/config.h"

typedef struct {
    Font light;
    Font regular;
    Font bold;
    Font mono;
} FontSet;

void assets_load_fonts(FontSet *fonts, const Config *config);
/* 0 when there is not enough memory for even one full-screen background. */
int assets_load_entries(EntryList *entries, const Image *screen);

#endif
