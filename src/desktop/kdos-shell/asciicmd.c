/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-ascii — a picture, as characters
 *
 *   $ grim -t ppm - | kdos-ascii
 *   $ kdos-ascii shot.ppm --width 100 --plain
 *
 * The CPU half of the Super+A effect, and the thing that makes it a claim
 * rather than a demo: the compositor's GLES2 pass and this share ONE algorithm
 * in libkcell — the same candidate table, the same six discs, the same contrast
 * pass, the same distance metric. When they disagree, one of them is wrong.
 *
 * It lives in kdos-shell rather than in kdos-tools for a dependency reason and
 * not an organisational one: measuring a glyph's shape needs the glyph, which
 * needs fcft, which kdos-shell already links through libkcell. Putting it in
 * `kdos` would have dragged fcft and pixman onto a binary that is otherwise
 * `kdos-getty` and `ksvc`.
 *
 * PPM IN, TEXT OUT. No PNG decoder: grim writes `-t ppm` and P6 is twenty lines
 * to parse, against libpng plus a second copy of the error handling wallpaper.c
 * already carries.
 * ---------------------------------
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kcell.h"
#include "shell.h"

/* ── P6 ────────────────────────────────────────────────────────────────── */

/* Skip whitespace and `#` comments, which netpbm allows anywhere in the
 * header — grim does not emit them, but a file that has been through anything
 * else might. */
static int pnm_next_int(FILE *f, int *out)
{
	int c, v = 0;
	bool got = false;

	for (;;) {
		c = fgetc(f);
		if (c == EOF)
			return -1;
		if (c == '#') {
			while (c != '\n' && c != EOF)
				c = fgetc(f);
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (got)
				break;
			continue;
		}
		if (c < '0' || c > '9')
			return -1;
		v = v * 10 + (c - '0');
		got = true;
	}
	if (!got)
		return -1;
	*out = v;
	return 0;
}

static uint32_t *read_ppm(FILE *f, int *w, int *h)
{
	if (fgetc(f) != 'P' || fgetc(f) != '6')
		return NULL;
	int maxv = 0;
	if (pnm_next_int(f, w) || pnm_next_int(f, h) || pnm_next_int(f, &maxv))
		return NULL;
	if (*w <= 0 || *h <= 0 || maxv != 255)
		return NULL;

	size_t n = (size_t)*w * (size_t)*h;
	uint8_t *rgb = malloc(n * 3);
	uint32_t *argb = malloc(n * 4);
	if (!rgb || !argb) {
		free(rgb);
		free(argb);
		return NULL;
	}
	if (fread(rgb, 3, n, f) != n) {
		free(rgb);
		free(argb);
		return NULL;
	}
	for (size_t i = 0; i < n; i++)
		argb[i] = 0xff000000u | ((uint32_t)rgb[i * 3] << 16) |
			  ((uint32_t)rgb[i * 3 + 1] << 8) | rgb[i * 3 + 2];
	free(rgb);
	return argb;
}

/* ── output ────────────────────────────────────────────────────────────── */

static void put_utf8(uint32_t cp)
{
	if (cp < 0x80) {
		putchar((int)cp);
	} else if (cp < 0x800) {
		putchar((int)(0xc0 | (cp >> 6)));
		putchar((int)(0x80 | (cp & 0x3f)));
	} else if (cp < 0x10000) {
		putchar((int)(0xe0 | (cp >> 12)));
		putchar((int)(0x80 | ((cp >> 6) & 0x3f)));
		putchar((int)(0x80 | (cp & 0x3f)));
	} else {
		putchar((int)(0xf0 | (cp >> 18)));
		putchar((int)(0x80 | ((cp >> 12) & 0x3f)));
		putchar((int)(0x80 | ((cp >> 6) & 0x3f)));
		putchar((int)(0x80 | (cp & 0x3f)));
	}
}

int asciicmd_main(int argc, char **argv)
{
	const char *path = NULL;
	const char *font = NULL;
	int want_cols = 0;
	bool plain = false, mono = false;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--width") && i + 1 < argc)
			want_cols = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--plain"))
			plain = true;		/* no colour at all */
		else if (!strcmp(argv[i], "--mono"))
			mono = true;		/* the accent, not the picture */
		else if (argv[i][0] != '-' && !path)
			path = argv[i];
		else {
			fprintf(stderr,
				"usage: kdos-ascii [FILE.ppm] [--width N] "
				"[--plain] [--mono] [--font NAME]\n"
				"       grim -t ppm - | kdos-ascii\n");
			return 2;
		}
	}

	FILE *f = path ? fopen(path, "rb") : stdin;
	if (!f) {
		fprintf(stderr, "kdos-ascii: cannot open %s\n", path);
		return 1;
	}
	int w = 0, h = 0;
	uint32_t *img = read_ppm(f, &w, &h);
	if (path)
		fclose(f);
	if (!img) {
		fprintf(stderr, "kdos-ascii: not a binary PPM (P6, maxval 255)\n");
		return 1;
	}

	/*
	 * The desktop's own font, so the glyph shapes the matcher measures are
	 * the ones the screen would have drawn. A different font is a different
	 * candidate set and a different picture.
	 */
	if (kcell_font_load(font ? font : "Terminus:pixelsize=32") != 0) {
		fprintf(stderr, "kdos-ascii: no usable font\n");
		free(img);
		return 1;
	}

	int cw = kcell_w(), ch = kcell_h();
	/*
	 * `--width` rescales the CELL rather than the image. Resampling the
	 * picture would need a filter and would change what the matcher sees;
	 * asking for bigger cells asks the same question at a coarser grid,
	 * which is what a width request actually means.
	 */
	if (want_cols > 0 && want_cols < w) {
		cw = w / want_cols;
		if (cw < 2)
			cw = 2;
		/* Keep the cell's aspect, or the picture stretches. */
		ch = cw * kcell_h() / kcell_w();
		if (ch < 2)
			ch = 2;
	}

	int cols = w / cw, rows = h / ch;
	if (cols < 1 || rows < 1) {
		fprintf(stderr, "kdos-ascii: image smaller than one cell\n");
		free(img);
		kcell_font_free();
		return 1;
	}

	uint32_t *cp = malloc((size_t)cols * rows * 4);
	uint32_t *tint = malloc((size_t)cols * rows * 4);
	if (!cp || !tint) {
		free(cp);
		free(tint);
		free(img);
		kcell_font_free();
		return 1;
	}

	int oc = 0, orow = 0;
	if (kcell_ascii_image(img, w, h, w, cw, ch, cp, tint, &oc, &orow) != 0) {
		fprintf(stderr, "kdos-ascii: could not measure the font\n");
		free(cp);
		free(tint);
		free(img);
		kcell_font_free();
		return 1;
	}

	KRgb accent = ktui_theme->slot[KT_ACCENT];

	for (int y = 0; y < orow; y++) {
		for (int x = 0; x < oc; x++) {
			size_t i = (size_t)y * oc + x;
			if (!plain) {
				uint32_t t = tint[i];
				int r = mono ? accent.r : (int)((t >> 16) & 0xff);
				int g = mono ? accent.g : (int)((t >> 8) & 0xff);
				int b = mono ? accent.b : (int)(t & 0xff);
				printf("\033[38;2;%d;%d;%dm", r, g, b);
			}
			put_utf8(cp[i]);
		}
		if (!plain)
			fputs("\033[0m", stdout);
		putchar('\n');
	}

	free(cp);
	free(tint);
	free(img);
	kcell_font_free();
	return 0;
}
