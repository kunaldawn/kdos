/*
 * kicon_slot_ink() through kch_header() — a header's picture can be seen.
 *
 * WHAT IT ASSERTS. The header band is a KT_ACCENT plate, and most of the atlas
 * is one-colour symbolic artwork whose tint stays light, so a header that drew
 * it through kicon_slot() drew light on light. For every accent, warmed and
 * not, each one-colour name a shipped header asks for is drawn by kch_header()
 * itself and the picture it registered is composited over the plate: its
 * strongest pixel must clear 4.5:1 against the plate. A multi-colour picture
 * asked for with an ink must come back pixel for pixel what kicon_slot()
 * draws, because its colours are the picture.
 *
 * WHY THE PICTURE AND NOT A GOLDEN. Every dump turns icons off — a golden is
 * the character grid — so no reference frame holds a pixel of a header icon.
 *
 * The argument is a data directory holding kdos/icons/atlas.kia; the committed
 * atlas is the one the image ships. Not shipped: testing/selftest.sh compiles
 * it and nothing else does.
 */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <pixman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kchrome.h"

/* Named by the chrome's own file and not reached by kch_header. */
int kch_body_slot(void) { return KT_SURFACE; }
int kch_px_text(int cx, int cy, int cw, int ch, const char *s, int fg, int bg,
		int align)
{
	(void)cx; (void)cy; (void)cw; (void)ch; (void)s; (void)fg; (void)bg;
	(void)align;
	return 0;
}

/* Every header name, kdos-shell's and kdos-res's pages', that is one colour
 * in the atlas at the size a 2x1 picture on a 16x32 cell asks for. */
static const char *const MASKS[] = {
	"lock", "package", "clock", "document-edit-verify", "system-upgrade",
	"system-users", "system-run", "view-statistics", "window",
};
/* Headers whose names carry colours of their own. */
static const char *const COLOURED[] = {
	"printer", "drive-harddisk", "media-optical-data",
};

static int fails;

static void ck(int cond, const char *what, const char *a, const char *b)
{
	if (!cond) {
		printf("  FAIL %s (%s%s%s)\n", what, a, b ? ", " : "",
		       b ? b : "");
		fails++;
	}
}

static double lin(double c)
{
	c /= 255.0;
	return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static double lum(double r, double g, double b)
{
	return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b);
}

static double ratio(double l1, double l2)
{
	return l1 > l2 ? (l1 + 0.05) / (l2 + 0.05) : (l2 + 0.05) / (l1 + 0.05);
}

/* The strongest contrast any pixel of the picture makes with the plate it is
 * drawn on, composited as the display composites it. */
static double best_on(pixman_image_t *img, KRgb plate)
{
	int w = pixman_image_get_width(img), h = pixman_image_get_height(img);
	int stride = pixman_image_get_stride(img) / 4;
	const uint32_t *px = pixman_image_get_data(img);
	double lp = lum(plate.r, plate.g, plate.b), best = 1.0;

	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			uint32_t p = px[y * stride + x];
			unsigned a = p >> 24;

			if (!a)
				continue;
			double k = (255.0 - a) / 255.0;
			double r = ((p >> 16) & 0xff) + plate.r * k;
			double g = ((p >> 8) & 0xff) + plate.g * k;
			double b = (p & 0xff) + plate.b * k;
			double c = ratio(lum(r, g, b), lp);

			if (c > best)
				best = c;
		}
	return best;
}

static pixman_image_t *pix_of(int slot)
{
	const KtuiSprite *s = slot >= 0 ? ktui_sprite_get(slot) : NULL;

	return s ? (pixman_image_t *)s->pix : NULL;
}

static int same_pixels(pixman_image_t *a, pixman_image_t *b)
{
	int w = pixman_image_get_width(a), h = pixman_image_get_height(a);

	if (w != pixman_image_get_width(b) || h != pixman_image_get_height(b))
		return 0;
	for (int y = 0; y < h; y++)
		if (memcmp((const char *)pixman_image_get_data(a) +
				   (size_t)y * pixman_image_get_stride(a),
			   (const char *)pixman_image_get_data(b) +
				   (size_t)y * pixman_image_get_stride(b),
			   (size_t)w * 4))
			return 0;
	return 1;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: iconink DATADIR\n");
		return 2;
	}
	setenv("XDG_DATA_HOME", argv[1], 1);
	setenv("XDG_DATA_DIRS", argv[1], 1);
	if (ktui_offscreen_init(40, 6) != 0) {
		printf("  iconink: no offscreen buffer\n");
		return 1;
	}
	ktui_draw_init();
	kicon_init(16, 32, 1);
	if (!kicon_enabled()) {
		printf("  FAIL the committed atlas did not open\n");
		return 1;
	}

	double worst = 1e9;
	char where[96];

	for (int t = 0; t < ktui_ntheme; t++)
		for (int night = 0; night < 2; night++) {
			ktui_theme_set(ktui_themes[t].name);
			ktui_theme_night(night);
			kicon_retint();
			snprintf(where, sizeof(where), "%s%s",
				 ktui_themes[t].name, night ? " warmed" : "");
			for (size_t i = 0; i < sizeof(MASKS) / sizeof(*MASKS);
			     i++) {
				int w, h;

				ktui_draw_clear();
				kch_header(40, MASKS[i], "Title", "sub", 1);
				const KtuiCell *c = ktui_draw_cells(&w, &h);
				uint32_t ch = c[1 * w + 2].ch;
				pixman_image_t *img =
					KTUI_IS_SPRITE(ch)
						? pix_of((int)KTUI_SPRITE_SLOT(ch))
						: NULL;

				ck(img != NULL, "the header drew no picture",
				   MASKS[i], where);
				if (!img)
					continue;
				double r = best_on(img,
						   ktui_theme->slot[KT_ACCENT]);
				if (r < worst)
					worst = r;
				ck(r >= 4.5,
				   "a one-colour header picture is under 4.5:1 "
				   "on its plate",
				   MASKS[i], where);
			}
			KRgb k = ktui_theme->slot[KT_SURFACE];
			uint32_t ink = ((uint32_t)k.r << 16) |
				       ((uint32_t)k.g << 8) | k.b;
			for (size_t i = 0;
			     i < sizeof(COLOURED) / sizeof(*COLOURED); i++) {
				pixman_image_t *a = pix_of(
					kicon_slot(COLOURED[i], 2, 1));
				pixman_image_t *b = pix_of(kicon_slot_ink(
					COLOURED[i], 2, 1, ink));

				ck(a && b, "a coloured picture did not load",
				   COLOURED[i], where);
				if (a && b)
					ck(same_pixels(a, b),
					   "an ink changed a picture of "
					   "several colours",
					   COLOURED[i], where);
			}
		}

	kicon_finish();
	if (fails)
		return 1;
	printf("  kicon_slot_ink: header masks read at %.2f:1 or better in "
	       "every accent, warmed or not; coloured pictures untouched\n",
	       worst);
	return 0;
}
