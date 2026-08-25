/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   --dump-cells — the frame as CELLS, colours included
 *
 * A plain `--dump` proves the layout and says nothing about whether the
 * surface is wearing the palette: a selection that stopped being an accent
 * fill, a label that moved from KT_MID to KT_DIM and became unreadable, and a
 * glyph drawn in the background's own slot are all byte-identical to a text
 * dump. One line per painted cell — `row col U+XXXX fg bg attr` — is the other
 * half of the golden-frame contract.
 *
 * IT GOES THROUGH A BACKEND rather than through ktui_offscreen_init(): the
 * cell buffer is private to libktui and the backend vtable is its documented
 * seam, and offscreen mode short-circuits the flush entirely.
 *
 * ONE COPY. This was written three times — kdos-keys, kdos-doc and
 * kdos-openwith each carried the same forty-five lines — which is three
 * answers to what a cell dump looks like and two of them nobody was reading.
 * A golden compares text: the moment two surfaces print it differently, half
 * the goldens are of a format the other half does not use.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

#include "shell.h"

static int cap_w = 80, cap_h = 24;

static void cap_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h, int ff)
{
	(void)prev;
	(void)ff;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const KtuiCell *c = &cur[y * w + x];

			/* Blanks are the background and there are thousands of
			 * them; a golden of those is a golden of the surface's
			 * SIZE, which the text dump already asserts. */
			if (!c->ch || c->ch == ' ' || c->ch == KTUI_WIDE_CONT)
				continue;
			printf("%d %d U+%04X %d %d %d\n", y, x, c->ch, c->fg,
			       c->bg, c->attr);
		}
}

static int cap_poll(KtuiEvent *ev, int timeout_ms)
{
	(void)ev;
	(void)timeout_ms;
	return 0;
}

static void cap_size(int *w, int *h)
{
	*w = cap_w;
	*h = cap_h;
}

/*
 * The RICH tier, deliberately. A cell dump is read by a diff rather than by a
 * person, and pinning the caps means a golden cannot move because the harness
 * ran somewhere with a different terminal — the ramp a surface picks is
 * asserted in src/libs/selftest.c, which is the place for it.
 */
static int cap_caps(void)
{
	return KT_CAP_UTF8 | KT_CAP_TRUECOLOR;
}

static const KtuiBackend cells_backend = {
	"dump-cells", cap_flush, cap_poll, cap_size, cap_caps
};

const KtuiBackend *sh_cells_backend(int w, int h)
{
	if (w > 0)
		cap_w = w;
	if (h > 0)
		cap_h = h;
	return &cells_backend;
}
