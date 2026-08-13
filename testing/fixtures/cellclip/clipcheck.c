/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   clipcheck — kcell_paint must not write outside the buffer
 *
 * This is the regression net for the defect that crashed kdos-comp on the first
 * real window of the first ISO that had window frames in it.
 *
 * pixman clips a COMPOSITE to the destination but does NOT clip an OP_SRC FILL:
 * pixman_image_fill_rectangles intersects only with `common.clip_region`, and
 * pixman_image_create_bits leaves that disabled. Every caller of kcell_paint
 * paints ceil(w / cell) cells into a buffer only w pixels wide — the last cell
 * is meant to be clipped — so the background fill ran off the end of the
 * allocation and corrupted the heap. musl's allocator then faulted on an
 * unrelated free() several frames later, inside pixman_image_unref, which is
 * nowhere near the code at fault.
 *
 * ASAN CANNOT SEE THIS. The store happens inside libpixman's hand-written fill
 * loop, which is not instrumented. So the test is a GUARD REGION and a byte
 * comparison, which does not care whose code did the writing.
 *
 * The third case is the control: a buffer that IS a whole number of cells
 * overflows under neither the broken nor the fixed code, so a test that only
 * checked that one would pass either way.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"

#define GUARD 4096

static int fails;

static void run(const char *what, int w, int h, int cols, int rows)
{
	size_t stride = (size_t)w * 4, sz = stride * (size_t)h;
	unsigned char *m = malloc(sz + GUARD);
	KtuiCell *c = calloc((size_t)cols * rows, sizeof(*c));
	if (!m || !c) {
		fails++;
		return;
	}
	memset(m, 0, sz);
	memset(m + sz, 0xAB, GUARD);
	for (int i = 0; i < cols * rows; i++) {
		c[i].ch = 0x2551;	/* a glyph, so the composite runs too */
		c[i].fg = KT_ACCENT;
		c[i].bg = KT_SURFACE;
	}

	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, w, h,
						       (uint32_t *)m, (int)stride);
	kcell_paint(img, c, NULL, cols, rows, 1, 1, w, h);

	int bad = 0;
	for (size_t i = 0; i < GUARD; i++)
		if (m[sz + i] != 0xAB)
			bad++;

	printf("  %-26s %4dx%-4d grid %2dx%-3d  %4d bytes past the end  %s\n",
	       what, w, h, cols, rows, bad, bad ? "FAIL" : "ok");
	if (bad)
		fails++;

	pixman_image_unref(img);
	free(c);
	free(m);
}

int main(int argc, char **argv)
{
	const char *font = argc > 1 ? argv[1] : "monospace:pixelsize=16";

	if (kcell_font_load(font) != 0) {
		fprintf(stderr, "clipcheck: no usable font\n");
		return 2;
	}
	int cw = kcell_w(), ch = kcell_h();
	printf("clipcheck — cell %dx%d\n", cw, ch);

	/* A side strip: one cell wide, a height that is NOT a whole number of
	 * cells, painted with ceil(h/cell) rows — deco.c's own arithmetic. */
	int gh = ch * 21 + 7;
	run("side strip, ragged height", cw, gh, 1, (gh + ch - 1) / ch);

	/* A top strip: one cell tall, a width that is not a whole number. */
	int fw = cw * 52 + 3;
	run("top strip, ragged width", fw, ch, (fw + cw - 1) / cw, 1);

	/* The control: exact multiples overflow under neither version. */
	run("exact multiple (control)", cw, ch * 21, 1, 21);

	kcell_font_free();
	printf("\n%s (%d failed)\n", fails ? "FAILED" : "all ok", fails);
	return fails ? 1 : 0;
}
