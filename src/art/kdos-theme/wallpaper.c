/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the wallpapers
 *
 *   wallpaper <outdir> [accent] --src MARK.png [--size WxH]...
 *
 * One Plasma wallpaper package per accent, <outdir>/KDOS-<Title>/:
 * metadata.json, contents/images_dark/<W>x<H>.png (the mark on a dark
 * ground) and contents/images/<W>x<H>.png (the same composition on the light
 * scheme's ground). A package with both directories follows Plasma's
 * light/dark switch by itself, so nothing KDOS runs has to swap a wallpaper
 * when the colour scheme flips.
 *
 * THE SOURCE IS THE MARK WITH ITS ALPHA, never a flattened wallpaper. The
 * penguin's body is black and so is a dark wallpaper's ground; once they are
 * flattened together nothing can tell them apart, and a light composition
 * needs the ground replaced while the body stays black.
 *
 * THE PENGUIN IS THE SAME IN EVERY ACCENT; ITS LIGHT IS THE ACCENT. Only the
 * pixels in libkcolor's accent family — the glow and the wordmark — are
 * recoloured, and each keeps its own lightness and saturation RELATIVE to
 * phosphor's primary, the colour the master is drawn in. Black, white and the
 * beak are the mark's own and stay as drawn.
 *
 * THE ACCENT IS SOLVED AGAINST THE GROUND IT IS DRAWN ON, as composited. The
 * wordmark is drawn at partial coverage, so its colour is held to 3:1 (WCAG
 * 1.4.11, non-text) as kcol_over() lays it on the ground, at the coverage the
 * wordmark has — the commonest alpha among the accent pixels that are more
 * than half opaque. Bone's primary on the light ground measures about 1.1:1
 * and would leave a light wallpaper with no wordmark at all; solved, it
 * walks darker until it reads.
 * ---------------------------------
 */

#include <fcntl.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "kdos-theme.h"

/* The sizes a package carries when the caller names none. Plasma picks the
 * one nearest the screen in aspect and area and scales it to fill, so the set
 * covers 16:9, 16:10 and a portrait screen, and stops at the largest size the
 * mark can be drawn at without being enlarged. */
static const KthSize DEFAULT_SIZES[] = {
	{ 1920, 1080 }, { 2560, 1600 }, { 3840, 2160 }, { 1080, 1920 },
};

KthSize kth_wallpaper_largest(void)
{
	KthSize best = DEFAULT_SIZES[0];

	for (size_t i = 1; i < sizeof(DEFAULT_SIZES) / sizeof(DEFAULT_SIZES[0]); i++)
		if ((long)DEFAULT_SIZES[i].w * DEFAULT_SIZES[i].h >
		    (long)best.w * best.h)
			best = DEFAULT_SIZES[i];
	return best;
}

/* The mark's height as a share of the screen's, capped by its width as a
 * share of the screen's, and NEVER ENLARGED: an enlarged raster mark is a
 * soft one. 62% of 2160 rows is the master's own 1337 to within two, so the
 * largest default size draws the artwork 1:1 and every smaller one is an
 * area-averaged reduction; a size past that keeps the mark at 1:1 and lets
 * the ground grow round it. */
#define MARK_H	0.62
#define MARK_W	0.70

/* The floor the wordmark is held to, x100. */
#define INK_FLOOR	300

typedef struct {
	int w, h;
	float *px;	/* premultiplied RGBA, 0..1 */
} Img;

/* ── reading the mark ─────────────────────────────────────────────────── */

static uint8_t *read_png(const char *path, int *w, int *h)
{
	png_image im;

	memset(&im, 0, sizeof(im));
	im.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_file(&im, path))
		kb_die("%s: %s", path, im.message);
	im.format = PNG_FORMAT_RGBA;

	uint8_t *px = kb_calloc(1, PNG_IMAGE_SIZE(im));

	if (!png_image_finish_read(&im, NULL, px, 0, NULL))
		kb_die("%s: %s", path, im.message);
	*w = (int)im.width;
	*h = (int)im.height;
	png_image_free(&im);
	return px;
}

static int is_accent(uint32_t rgb)
{
	return kcol_family(rgb) == KCOL_FAM_ACCENT;
}

static uint8_t ink_coverage(const uint8_t *px, int n)
{
	unsigned count[256] = { 0 };
	int best = 255;

	for (int i = 0; i < n; i++) {
		const uint8_t *p = px + (size_t)i * 4;

		if (p[3] < 128)
			continue;
		if (is_accent((uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2]))
			count[p[3]]++;
	}
	for (int a = 128; a < 256; a++)
		if (count[a] > count[best])
			best = a;
	return (uint8_t)best;
}

/* ── colour ───────────────────────────────────────────────────────────── */

static double clamp01(double v)
{
	return v < 0.0 ? 0.0 : v > 1.0 ? 1.0 : v;
}

/* `ink` walked in lightness, away from the ground, until the wordmark laid
 * on the ground at `alpha` clears the floor. Lightness and not a mix towards
 * the scheme's text: a mix greys the accent out, a lightness walk keeps its
 * hue and saturation. */
static uint32_t solve_ink(uint32_t ink, uint32_t ground, uint8_t alpha)
{
	double h, l, s;
	int up = kcol_luma(ground) < 0.5;

	kcol_to_hls(ink, &h, &l, &s);
	for (int i = 0; i <= 100; i++) {
		uint32_t c = kcol_from_hls(h, l, s);

		if (kcol_contrast(kcol_over(c, ground, alpha), ground) >=
		    INK_FLOOR)
			return c;
		l = clamp01(l + (up ? 0.01 : -0.01));
	}
	return kcol_from_hls(h, up ? 1.0 : 0.0, s);
}

/* The accent pixel's offset from the reference, carried onto the ink: the
 * ink's hue, the pixel's lightness moved by as much as it differs from the
 * reference's, and its saturation in the same ratio. */
static uint32_t relight(uint32_t px, uint32_t ref, uint32_t ink)
{
	double ph, pl, ps, rh, rl, rs, ih, il, is;

	kcol_to_hls(px, &ph, &pl, &ps);
	kcol_to_hls(ref, &rh, &rl, &rs);
	kcol_to_hls(ink, &ih, &il, &is);

	double l = il + (pl - rl);

	/* Past white is rescaled into the headroom the ink has, so a glow
	 * brighter than the reference stays brighter than the ink instead of
	 * clipping to a flat white rim. */
	if (l > 1.0 && rl < 1.0)
		l = il + (1.0 - il) * (pl - rl) / (1.0 - rl);
	return kcol_from_hls(ih, clamp01(l), clamp01(rs > 0.0 ? is * ps / rs
							     : ps));
}

/*
 * The source recoloured for one ink, premultiplied.
 *
 * THE GLOW KEEPS THE ACCENT; THE INK IS WHAT IS READ. An accent pixel's
 * target runs from the accent itself at no coverage to the solved ink at the
 * wordmark's coverage, so the faint outer glow stays the accent's own colour
 * — a tint, which is what reads as a glow on a light ground — while the
 * wordmark carries the colour that clears the floor. Solving the glow too
 * turns it into a grey shadow on a light ground. On a dark ground the ink of
 * a dark scheme is its primary, and the two ends are the same colour.
 *
 * The master has a few thousand distinct colours at a few hundred coverages,
 * so each pair is converted once.
 */
static void recolour(const uint8_t *src, int w, int h, uint32_t ref,
		     uint32_t accent, uint32_t ink, uint8_t cover, Img *out)
{
	enum { CACHE = 1 << 15 };
	uint32_t *key = kb_calloc(CACHE, sizeof(uint32_t));
	uint32_t *val = kb_calloc(CACHE, sizeof(uint32_t));

	memset(key, 0xff, CACHE * sizeof(uint32_t));
	out->w = w;
	out->h = h;
	out->px = kb_calloc((size_t)w * (size_t)h * 4, sizeof(float));

	for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
		const uint8_t *p = src + i * 4;
		float *o = out->px + i * 4;

		if (!p[3])
			continue;

		uint32_t c = (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
		int pct = p[3] >= cover ? 100 : p[3] * 100 / cover;
		uint32_t k = c | (uint32_t)pct << 24;
		unsigned slot = (k ^ (k >> 11) ^ (k >> 23)) & (CACHE - 1);
		uint32_t v;

		if (key[slot] == k) {
			v = val[slot];
		} else {
			v = is_accent(c) ? relight(c, ref, kcol_mix(accent, ink,
								    pct))
					 : c;
			key[slot] = k;
			val[slot] = v;
		}

		float a = (float)p[3] / 255.0f;

		o[0] = (float)((v >> 16) & 0xff) / 255.0f * a;
		o[1] = (float)((v >> 8) & 0xff) / 255.0f * a;
		o[2] = (float)(v & 0xff) / 255.0f * a;
		o[3] = a;
	}
	free(key);
	free(val);
}

/* ── resampling ───────────────────────────────────────────────────────── */

/* One output sample's taps along one axis. */
typedef struct {
	int first, n;
	float *w;
} Taps;

/*
 * AREA AVERAGING going down, bilinear going up. The wordmark is pixel art;
 * a filter with negative lobes (Lanczos, bicubic) rings on its hard edges
 * and gives blocks of different widths, while averaging the exact area each
 * output pixel covers is the right operation for a reduction.
 */
static Taps *taps(int src, int dst)
{
	Taps *t = kb_calloc((size_t)dst, sizeof(Taps));
	double f = (double)src / (double)dst;

	for (int x = 0; x < dst; x++) {
		if (f >= 1.0) {
			double lo = x * f, hi = (x + 1) * f;
			int a = (int)lo, b = (int)hi;

			if (b >= src)
				b = src - 1;
			t[x].first = a;
			t[x].n = b - a + 1;
			t[x].w = kb_calloc((size_t)t[x].n, sizeof(float));
			for (int i = a; i <= b; i++) {
				double s = i > lo ? i : lo;
				double e = i + 1 < hi ? i + 1 : hi;

				t[x].w[i - a] = (float)((e > s ? e - s : 0) / f);
			}
		} else {
			double u = (x + 0.5) * f - 0.5;
			int a = (int)(u < 0 ? 0 : u);
			double fr = u - a;

			if (u < 0)
				fr = 0;
			if (a >= src - 1) {
				a = src - 1;
				fr = 0;
			}
			t[x].first = a;
			t[x].n = fr > 0 ? 2 : 1;
			t[x].w = kb_calloc(2, sizeof(float));
			t[x].w[0] = (float)(1.0 - fr);
			t[x].w[1] = (float)fr;
		}
	}
	return t;
}

static void taps_free(Taps *t, int n)
{
	for (int i = 0; i < n; i++)
		free(t[i].w);
	free(t);
}

static void resample(const Img *in, int dw, int dh, Img *out)
{
	Taps *tx = taps(in->w, dw), *ty = taps(in->h, dh);
	float *mid = kb_calloc((size_t)dw * (size_t)in->h * 4, sizeof(float));

	for (int y = 0; y < in->h; y++) {
		const float *row = in->px + (size_t)y * in->w * 4;
		float *o = mid + (size_t)y * dw * 4;

		for (int x = 0; x < dw; x++) {
			float acc[4] = { 0 };

			for (int k = 0; k < tx[x].n; k++) {
				const float *p = row + (size_t)(tx[x].first + k) * 4;

				for (int c = 0; c < 4; c++)
					acc[c] += p[c] * tx[x].w[k];
			}
			memcpy(o + (size_t)x * 4, acc, sizeof(acc));
		}
	}

	out->w = dw;
	out->h = dh;
	out->px = kb_calloc((size_t)dw * (size_t)dh * 4, sizeof(float));
	for (int y = 0; y < dh; y++) {
		float *o = out->px + (size_t)y * dw * 4;

		for (int k = 0; k < ty[y].n; k++) {
			const float *row = mid + (size_t)(ty[y].first + k) * dw * 4;
			float w = ty[y].w[k];

			for (int i = 0; i < dw * 4; i++)
				o[i] += row[i] * w;
		}
	}
	free(mid);
	taps_free(tx, dw);
	taps_free(ty, dh);
}

/* ── writing ──────────────────────────────────────────────────────────── */

static uint8_t to8(float v)
{
	float x = v * 255.0f + 0.5f;

	return x <= 0.0f ? 0 : x >= 255.0f ? 255 : (uint8_t)x;
}

/* RGB, no alpha: a wallpaper is opaque, and a quarter of the bytes. No
 * time chunk is written, so the same inputs give the same file. */
static void write_png(const char *path, const uint8_t *rgb, int w, int h)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	FILE *f = fd >= 0 ? fdopen(fd, "wb") : NULL;

	if (!f)
		kb_die("cannot write %s", path);

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
						  NULL, NULL);
	png_infop info = png ? png_create_info_struct(png) : NULL;

	if (!info)
		kb_die("libpng could not start a write");
	if (setjmp(png_jmpbuf(png)))
		kb_die("libpng failed writing %s", path);

	png_init_io(png, f);
	png_set_compression_level(png, 9);
	png_set_IHDR(png, info, (png_uint_32)w, (png_uint_32)h, 8,
		     PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);
	for (int y = 0; y < h; y++)
		png_write_row(png, (png_const_bytep)(rgb + (size_t)y * w * 3));
	png_write_end(png, NULL);
	png_destroy_write_struct(&png, &info);
	if (fclose(f) != 0)
		kb_die("cannot write %s", path);
}

static void compose(const Img *mark, int W, int H, uint32_t ground,
		    const char *path)
{
	double sc = MARK_H * H / mark->h;
	double sw = MARK_W * W / mark->w;

	if (sw < sc)
		sc = sw;
	if (sc > 1.0)
		sc = 1.0;

	int mw = (int)(mark->w * sc + 0.5), mh = (int)(mark->h * sc + 0.5);

	if (mw < 1)
		mw = 1;
	if (mh < 1)
		mh = 1;

	Img m;

	resample(mark, mw, mh, &m);

	float g[3] = {
		(float)((ground >> 16) & 0xff) / 255.0f,
		(float)((ground >> 8) & 0xff) / 255.0f,
		(float)(ground & 0xff) / 255.0f,
	};
	uint8_t g8[3] = { to8(g[0]), to8(g[1]), to8(g[2]) };
	uint8_t *rgb = kb_calloc((size_t)W * (size_t)H, 3);
	int x0 = (W - mw) / 2, y0 = (H - mh) / 2;

	for (size_t i = 0; i < (size_t)W * (size_t)H; i++)
		memcpy(rgb + i * 3, g8, 3);
	for (int y = 0; y < mh; y++) {
		int ty = y0 + y;

		if (ty < 0 || ty >= H)
			continue;
		for (int x = 0; x < mw; x++) {
			int tx = x0 + x;

			if (tx < 0 || tx >= W)
				continue;
			const float *p = m.px + ((size_t)y * mw + x) * 4;
			uint8_t *o = rgb + ((size_t)ty * W + tx) * 3;
			float a = p[3] > 1.0f ? 1.0f : p[3];

			for (int c = 0; c < 3; c++)
				o[c] = to8(p[c] + g[c] * (1.0f - a));
		}
	}
	write_png(path, rgb, W, H);
	free(rgb);
	free(m.px);
}

static void metadata(const char *dir, const KcolScheme *sc)
{
	KbBuf b = {0}, id = {0}, name = {0};

	kb_buf_printf(&id, "%s", sc->theme_name);
	kb_buf_printf(&name, "KDOS %s", kth_title(sc));
	kb_buf_str(&b, "{\n    \"KPlugin\": {\n"
		       "        \"Authors\": [\n"
		       "            {\n"
		       "                \"Name\": \"KDOS\"\n"
		       "            }\n"
		       "        ],\n"
		       "        \"Id\": ");
	kb_json_str(&b, id.p);
	kb_buf_str(&b, ",\n        \"License\": \"MIT\",\n        \"Name\": ");
	kb_json_str(&b, name.p);
	kb_buf_str(&b, "\n    }\n}\n");
	kth_emit(dir, "metadata.json", &b);
	kb_buf_free(&b);
	kb_buf_free(&id);
	kb_buf_free(&name);
}

int kth_wallpaper(const char *outdir, const char *accent, const char *src,
		  const KthSize *sizes, int nsize)
{
	const KcolScheme *pick[32];
	int n = kth_pick(accent, pick, 32);
	const KcolScheme *ph = kcol_find("phosphor");

	if (!ph)
		kb_die("the palette has no phosphor row, which the mark is "
		       "drawn in");
	if (!sizes || !nsize) {
		sizes = DEFAULT_SIZES;
		nsize = (int)(sizeof(DEFAULT_SIZES) / sizeof(DEFAULT_SIZES[0]));
	}

	int w, h;
	uint8_t *mark = read_png(src, &w, &h);
	uint8_t alpha = ink_coverage(mark, w * h);
	uint32_t light = kth_light_scheme()->deep;

	for (int i = 0; i < n; i++) {
		const KcolScheme *sc = pick[i];
		char *pkg = kb_path_join(outdir, sc->theme_name);
		const struct {
			const char *dir;
			uint32_t ground;
		} var[2] = {
			{ "contents/images_dark", kth_dark_partner(sc)->deep },
			{ "contents/images", light },
		};

		metadata(pkg, sc);
		for (int v = 0; v < 2; v++) {
			uint32_t ink = solve_ink(sc->primary, var[v].ground,
						 alpha);
			char *dir = kb_path_join(pkg, var[v].dir);
			Img src_img;

			if (kb_mkdir_p(dir) < 0)
				kb_die("cannot create %s", dir);
			recolour(mark, w, h, ph->primary, sc->primary, ink,
				 alpha, &src_img);
			for (int s = 0; s < nsize; s++) {
				char file[64];

				snprintf(file, sizeof(file), "%dx%d.png",
					 sizes[s].w, sizes[s].h);
				char *path = kb_path_join(dir, file);

				compose(&src_img, sizes[s].w, sizes[s].h,
					var[v].ground, path);
				free(path);
			}
			free(src_img.px);
			free(dir);
		}
		free(pkg);
	}
	free(mark);
	return 0;
}
