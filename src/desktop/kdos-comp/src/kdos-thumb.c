// SPDX-License-Identifier: GPL-2.0-only
/*
 * A WINDOW'S PIXELS, AS A FILE — what a hover preview is made of.
 *
 * The taskbar is icons-only, so a tooltip carries the whole of what a button
 * is. A name is most of that; a picture of the window is the rest, and it is
 * the one thing this desktop can offer that a name cannot. Windows 7 put
 * thumbnails on its taskbar for exactly this reason and its designers were
 * candid about the limit — "we know these small previews don't always provide
 * enough information" — which is why here the preview is an ADDITION to the
 * text and never a replacement for it.
 *
 * IT IS DRAWN AS CHARACTERS at the other end. kdos-tip runs the result through
 * libkcell's shape-vector renderer, so a preview on this desktop is a grid of
 * cells like everything else rather than a photograph pasted into a text-mode
 * bar. That is also why the geometry here is tiny: the consumer is going to
 * reduce it to a few dozen cells regardless.
 *
 * WHY A FILE AND NOT A PROTOCOL. The ecosystem answer is
 * ext-image-copy-capture-v1 with a foreign-toplevel source, and a client of it
 * is a session, a frame, a buffer negotiation and an async ready callback —
 * for a picture that is about to be thrown away at 64x32. This is one distro's
 * channel between two of its own programs, which is what kdos-frames.sock and
 * kdos-cmd.sock already are; the file lives in $XDG_RUNTIME_DIR, is overwritten
 * every time, and nothing keeps it.
 *
 * WHAT IT CANNOT DO, said out loud: a client that renders into a dmabuf — any
 * GPU-accelerated toolkit — has no host-mappable pixels, and
 * wlr_buffer_begin_data_ptr_access() correctly refuses. There is no preview for
 * those windows and the tooltip is simply the text, which is what every caller
 * of this must be built to accept. Getting a picture from them means the
 * renderer reading the texture back, and that is a different piece of work.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>

#include "labwc.h"
#include "view.h"
#include "kdos.h"

/* Small on purpose — see the header comment. Also a ceiling on what one
 * request can cost while a pointer is merely resting somewhere. */
#define THUMB_MAX_W 160
#define THUMB_MAX_H 100

static bool
fmt_is_argb(uint32_t fmt)
{
	/*
	 * The two shm formats every client uses, and the only two whose byte
	 * order this reads correctly. Anything else is refused rather than
	 * reinterpreted: a preview with its channels swapped is worse than no
	 * preview, because it looks like the compositor is broken.
	 */
	return fmt == DRM_FORMAT_ARGB8888 || fmt == DRM_FORMAT_XRGB8888;
}

/*
 * A BOX FILTER, because the reduction is enormous and the content is TEXT.
 *
 * A 1280x800 window into a 136x72 thumbnail is nine to one on each axis, so
 * nearest-neighbour keeps one pixel in eighty and throws away exactly what a
 * terminal is made of: a one-pixel stroke has a one-in-nine chance of being
 * the pixel that was kept, per axis. Measured — a foot window full of text
 * came out of the sampler as an empty grid with a single glyph in it.
 *
 * Averaging twice is not the objection it looks like. The consumer samples six
 * discs per CELL to recover the cell's SHAPE, which is a different question
 * from what colour a thumbnail pixel is, and a shape recovered from a
 * point-sampled source is the shape of the sampling grid rather than of the
 * window. It costs one pass over the source, once per request, on a request
 * that only happens because somebody's pointer came to rest.
 */
static void
downscale(const uint32_t *src, int sw, int sh, int sstride,
	uint32_t *dst, int dw, int dh)
{
	for (int y = 0; y < dh; y++) {
		int y0 = y * sh / dh, y1 = (y + 1) * sh / dh;

		if (y1 <= y0) {
			y1 = y0 + 1;
		}
		for (int x = 0; x < dw; x++) {
			int x0 = x * sw / dw, x1 = (x + 1) * sw / dw;
			uint32_t r = 0, g = 0, b = 0;
			uint32_t n;

			if (x1 <= x0) {
				x1 = x0 + 1;
			}
			for (int sy = y0; sy < y1; sy++) {
				const uint32_t *row = src + (size_t)sy * sstride;

				for (int sx = x0; sx < x1; sx++) {
					uint32_t p = row[sx];

					r += (p >> 16) & 0xff;
					g += (p >> 8) & 0xff;
					b += p & 0xff;
				}
			}
			n = (uint32_t)(y1 - y0) * (uint32_t)(x1 - x0);
			dst[y * dw + x] = 0xff000000u | ((r / n) << 16) |
					  ((g / n) << 8) | (b / n);
		}
	}
}

/*
 * Write the picture where the consumer can read it.
 *
 * Temp file plus rename, the same rule every other state file in this tree
 * keeps: the reader is a separate process that may open this at any moment,
 * and half a picture is worse than none — it decodes to garbage rather than
 * failing. The rename is what makes the swap atomic.
 */
static bool
write_ppm(const char *path, const uint32_t *px, int w, int h)
{
	char tmp[512];
	FILE *f;
	bool ok = true;

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	f = fopen(tmp, "wb");
	if (!f) {
		return false;
	}
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (int i = 0; i < w * h && ok; i++) {
		unsigned char rgb[3] = {
			(unsigned char)((px[i] >> 16) & 0xff),
			(unsigned char)((px[i] >> 8) & 0xff),
			(unsigned char)(px[i] & 0xff),
		};

		ok = fwrite(rgb, 1, 3, f) == 3;
	}
	if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
		ok = false;
	}
	fclose(f);
	if (!ok || rename(tmp, path) != 0) {
		unlink(tmp);
		return false;
	}
	return true;
}

/*
 * The most recently ACTIVATED view with this app_id, so a grouped button
 * previews the window the user last had in front rather than whichever the
 * list happens to yield first. Falls back to the first mapped one.
 */
static struct view *
pick(const char *app_id)
{
	struct view *view, *best = NULL;

	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NONE) {
		const char *id;

		if (!view->surface || !view->mapped) {
			continue;
		}
		id = view->app_id;
		if (!id || strcmp(id, app_id)) {
			continue;
		}
		if (!best || view == server.active_view) {
			best = view;
		}
	}
	return best;
}

bool
kdos_thumb_write(const char *app_id, int w, int h, const char *path)
{
	struct view *view = app_id ? pick(app_id) : NULL;
	struct wlr_buffer *buf;
	void *data = NULL;
	uint32_t fmt = 0;
	size_t stride = 0;
	uint32_t *small;
	int sw, sh;
	bool ok;

	if (!view || !view->surface || !view->surface->buffer) {
		return false;
	}
	if (w < 8 || h < 8 || w > THUMB_MAX_W || h > THUMB_MAX_H) {
		return false;
	}
	buf = &view->surface->buffer->base;
	sw = buf->width;
	sh = buf->height;
	if (sw < 1 || sh < 1) {
		return false;
	}
	if (!wlr_buffer_begin_data_ptr_access(buf,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &fmt,
			&stride)) {
		return false;	/* a dmabuf — see the header comment */
	}
	if (!fmt_is_argb(fmt) || stride % 4) {
		wlr_buffer_end_data_ptr_access(buf);
		return false;
	}
	small = malloc((size_t)w * h * sizeof(*small));
	if (!small) {
		wlr_buffer_end_data_ptr_access(buf);
		return false;
	}
	downscale(data, sw, sh, (int)(stride / 4), small, w, h);
	wlr_buffer_end_data_ptr_access(buf);

	ok = write_ppm(path, small, w, h);
	free(small);
	return ok;
}
