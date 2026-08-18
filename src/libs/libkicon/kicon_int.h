/* libkicon internals. Not installed, not included by any consumer. */

#ifndef KICON_INT_H
#define KICON_INT_H

#include <stdint.h>
#include <stddef.h>

#include "kicon.h"

/* kicon_png.c — straight (NOT premultiplied) RGBA8888, malloc'd. */
uint8_t *ki_png_file(const char *path, int *w, int *h);
uint8_t *ki_png_mem(const void *data, size_t len, int *w, int *h);

/*
 * kicon_atlas.c — the container of PNG blobs.
 *
 *   "KIA1" u32 count
 *   count x { u16 nlen, u16 size, u32 off, u32 len }  then the names, packed
 *   the blobs
 *
 * A container of PNGs rather than raw pixels because a flat icon compresses to
 * about a kilobyte and the whole set otherwise runs to tens of megabytes in
 * git — and libpng is a dependency of this library regardless, for the app
 * icons. mmap'd, so nothing is read that nobody asks for.
 */
int ki_atlas_open(const char *path);
void ki_atlas_close(void);
int ki_atlas_ok(void);
/* The blob for the smallest size >= `want`, or the largest there is. NULL when
 * the name is not in the atlas at all. */
const void *ki_atlas_find(const char *name, int want, size_t *len, int *size);
int ki_atlas_count(void);

/*
 * The MIME half lives in LIBKXDG (kxdg_mime_for_path, kxdg_mime_icon_names).
 * It was going to be here and it belongs there: `openwith.c` needs exactly the
 * same glob resolution and cannot link this library — the dump harness that
 * renders it offscreen has no pixman and no libpng. One copy, in the archive
 * both consumers already link.
 */

#endif /* KICON_INT_H */
