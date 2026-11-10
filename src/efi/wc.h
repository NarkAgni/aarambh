#ifndef AARAMBH_EFI_WC_H
#define AARAMBH_EFI_WC_H

#include <efi.h>

/*
 * Map the framebuffer write-combining through the PAT, so frames reach an
 * uncached framebuffer many times faster. wc_disable() puts the memory type
 * of every page and the PAT back, and must run before another image starts.
 */
int wc_enable(UINT64 base, UINT64 size);
void wc_disable(void);

#endif
