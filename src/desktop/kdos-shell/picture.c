/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   A picture in a cell grid, for the surfaces that show one
 *
 * `kdos-peek` shows a page and `kdos-pix` shows a photograph, and everything
 * between the bytes and the cells is the same: decode through libkimg under a
 * budget, crop, scale, cut into sprite tiles, draw. This is that, once.
 *
 * THE PIECES THAT ARE EASY TO GET WRONG, and each of them cost a debugging
 * session before it was written down:
 *
 *   - `kcon_set_sprite_bits()` AFTER `kdisp_init`. The console backend clears
 *     its whole client state when it connects, so a callback registered before
 *     is erased — and the failure is not a missing picture but a BLANK one,
 *     because the session maps a slot it was never sent to -1 and the cell
 *     becomes a space.
 *   - A console surface has no pixel size of its own. The display it is drawn
 *     on has, and it rescales what arrives, so a nominal cell bounds the wire
 *     without pretending to know the font at the other end.
 *   - The new tile grid is registered BEFORE the old one is given back. The
 *     table hands a freed slot straight out again, so dropping first lets the
 *     next picture take the same slot numbers, the row diff sees nothing, and
 *     the screen keeps the previous one; the allocator does the same with the
 *     tiles' memory, so the picture behind a slot comes back as a pointer the
 *     display has already been sent.
 *   - Without an evictor the table holds a borrowed pointer forever: every
 *     re-put leaks a picture and the table fills.
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <pixman.h>

#include "kbase.h"
#include "kcell.h"
#include "kcon.h"
#include "kimg.h"
#include "kwl.h"
#include "shell.h"

/* Under the compositor the backend knows a cell's pixel size; as a console
 * surface this program has none at all. */
#define PIC_NOMINAL_CW 10
#define PIC_NOMINAL_CH 20

int sh_pic_cell_w(void)
{
	int w = kdisp_cell_w();

	return w > 1 ? w : PIC_NOMINAL_CW;
}

int sh_pic_cell_h(void)
{
	int h = kdisp_cell_h();

	return h > 1 ? h : PIC_NOMINAL_CH;
}

/*
 * WHERE A SPRITE'S PIXELS COME FROM when this is a console surface. libkcon
 * links no pixel library and must not; it asks through this and puts the bytes
 * on the wire, and the display on the other end scales them to whatever a cell
 * is there.
 */
static int sprite_bits(const void *pix, const uint32_t **argb, int *w, int *h,
		       int *stride_px, void *user)
{
	pixman_image_t *img = (pixman_image_t *)pix;

	(void)user;
	if (!img)
		return -1;
	*argb = pixman_image_get_data(img);
	*w = pixman_image_get_width(img);
	*h = pixman_image_get_height(img);
	*stride_px = pixman_image_get_stride(img) / 4;
	return *argb && *w > 0 && *h > 0 ? 0 : -1;
}

void sh_pic_backend(void)
{
	kcon_set_sprite_bits(sprite_bits, NULL);
	ktui_sprite_evictor(kcell_tile_free, NULL);
	ktui_sprite_budget(SH_PIC_BUDGET, sh_pic_cell_w(), sh_pic_cell_h());
}

/* Something rather than nothing where there are no pixels — a tty, a dump, a
 * view with no pixel library. A picture that rendered as blank cells cannot be
 * told apart from one that failed to arrive. */
static uint32_t fallback_cp(void)
{
	return (ktui_caps & KT_CAP_UTF8) ? 0x2593u : (uint32_t)'#';
}

static uint64_t fnv(const void *p, size_t n, uint64_t h)
{
	const unsigned char *b = p;

	for (size_t i = 0; i < n; i++) {
		h ^= b[i];
		h *= 0x100000001b3ULL;
	}
	return h;
}

unsigned char *sh_pic_slurp(const char *path, size_t *len)
{
	struct stat st;
	unsigned char *b;
	FILE *f = fopen(path, "rb");

	*len = 0;
	if (!f)
		return NULL;
	if (fstat(fileno(f), &st) != 0 || !S_ISREG(st.st_mode) ||
	    (unsigned long long)st.st_size > SH_PIC_MAX_FILE) {
		fclose(f);
		return NULL;
	}
	b = malloc((size_t)st.st_size ? (size_t)st.st_size : 1);
	if (!b) {
		fclose(f);
		return NULL;
	}
	*len = fread(b, 1, (size_t)st.st_size, f);
	fclose(f);
	return b;
}

/* The magic of the formats libkimg decodes. Sniffed before the file is read: a
 * two-gigabyte video must not be loaded to discover it is not a PNG. */
int sh_pic_is_image(const unsigned char *b, size_t n)
{
	if (n >= 8 && !memcmp(b, "\x89PNG\r\n\x1a\n", 8))
		return 1;
	if (n >= 3 && b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff)
		return 1;
	if (n >= 12 && !memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WEBP", 4))
		return 1;
	return 0;
}

void sh_pic_tiles_drop(ShPic *p)
{
	if (p->cw > 0)
		ktui_sprite_drop_tiled(p->key, p->cw, p->ch);
	p->cw = p->ch = 0;
}

void sh_pic_free(ShPic *p)
{
	sh_pic_tiles_drop(p);
	if (p->img)
		pixman_image_unref((pixman_image_t *)p->img);
	p->img = NULL;
	p->w = p->h = 0;
}

int sh_pic_set(ShPic *p, const unsigned char *b, size_t n)
{
	KimgBudget bud = { SH_PIC_MAX_W, SH_PIC_MAX_H, SH_PIC_MAX_PIX };
	pixman_image_t *img;

	/*
	 * The SOURCE is replaced here; the tiles are not. They are given back
	 * once the next picture's are registered — see sh_pic_view(). Each
	 * tile is an image of its own that the table owns, so unreffing this
	 * one takes none of them with it.
	 */
	if (p->img)
		pixman_image_unref((pixman_image_t *)p->img);
	p->img = NULL;
	img = kimg_decode(b, n, KIMG_AUTO, &bud);
	if (!img) {
		sh_pic_tiles_drop(p);
		p->w = p->h = 0;
		return -1;
	}
	p->img = img;
	p->w = pixman_image_get_width(img);
	p->h = pixman_image_get_height(img);
	/* The WHOLE buffer: two pages of a scan can share their first
	 * kilobytes, and a key that collided would draw the previous one. */
	p->id = fnv(b, n, 0xcbf29ce484222325ULL);
	p->id = fnv(&p->w, sizeof(p->w), p->id);
	return 0;
}

int sh_pic_load(ShPic *p, const char *path)
{
	unsigned char *b;
	size_t n = 0;
	int rc;

	b = sh_pic_slurp(path, &n);
	if (!b)
		return -1;
	rc = sh_pic_set(p, b, n);
	free(b);
	return rc;
}

/*
 * Fit a source rectangle to a pane in CELLS, never enlarging it: a 32-pixel
 * icon blown up to a window is a blur of what the file actually holds. A cell
 * is not square, so the two axes are converted through the cell's pixel size
 * rather than compared directly.
 */
void sh_pic_fit(int sw, int sh, int pane_w, int pane_h, int *cw, int *ch)
{
	int cellw = sh_pic_cell_w(), cellh = sh_pic_cell_h();
	long long maxw = (long long)pane_w * cellw;
	long long maxh = (long long)pane_h * cellh;
	long long dw = sw, dh = sh;

	*cw = *ch = 0;
	if (sw <= 0 || sh <= 0 || pane_w < 1 || pane_h < 1)
		return;
	if (dw > maxw) {
		dh = dh * maxw / dw;
		dw = maxw;
	}
	if (dh > maxh) {
		dw = dw * maxh / dh;
		dh = maxh;
	}
	*cw = (int)((dw + cellw - 1) / cellw);
	*ch = (int)((dh + cellh - 1) / cellh);
	if (*cw < 1)
		*cw = 1;
	if (*ch < 1)
		*ch = 1;
	if (*cw > pane_w)
		*cw = pane_w;
	if (*ch > pane_h)
		*ch = pane_h;
}

/*
 * Register `cw` by `ch` cells showing the source rectangle. A crop first and a
 * scale second, in two steps rather than one transform: the crop is at most the
 * source's own size and the code that does it is four lines, where a combined
 * scale-and-offset transform is the kind of arithmetic that is wrong by half a
 * pixel for a year.
 */
int sh_pic_view(ShPic *p, int sx, int sy, int sw, int sh, int cw, int ch)
{
	uint64_t key = p->id;
	uint64_t okey = p->key;
	int ocw = p->cw, och = p->ch;
	pixman_image_t *crop;
	uint32_t *bits;
	int ok;
	int box[6] = { sx, sy, sw, sh, cw, ch };

	if (!p->img || sw < 1 || sh < 1 || cw < 1 || ch < 1)
		return -1;
	if (sx < 0)
		sx = 0;
	if (sy < 0)
		sy = 0;
	if (sx + sw > p->w)
		sw = p->w - sx;
	if (sy + sh > p->h)
		sh = p->h - sy;
	if (sw < 1 || sh < 1)
		return -1;
	box[0] = sx;
	box[1] = sy;
	box[2] = sw;
	box[3] = sh;
	key = fnv(box, sizeof(box), key);
	if (p->cw == cw && p->ch == ch && p->key == key)
		return 0;

	bits = calloc((size_t)sw * (size_t)sh, 4);
	if (!bits)
		return -1;
	crop = pixman_image_create_bits(PIXMAN_a8r8g8b8, sw, sh, bits, sw * 4);
	if (!crop) {
		free(bits);
		return -1;
	}
	pixman_image_composite32(PIXMAN_OP_SRC, (pixman_image_t *)p->img, NULL,
				 crop, sx, sy, 0, 0, 0, 0, sw, sh);
	ok = kcell_tile_picture(crop, key, cw, ch, sh_pic_cell_w(),
				sh_pic_cell_h(), fallback_cp());
	pixman_image_unref(crop);
	free(bits);
	if (ok <= 0) {
		sh_pic_tiles_drop(p);
		return -1;
	}
	p->key = key;
	p->cw = cw;
	p->ch = ch;
	if (ocw > 0 && okey != key)
		ktui_sprite_drop_tiled(okey, ocw, och);
	return 0;
}

/* Tile by tile, because the table registers a picture as a GRID of sprites and
 * each one is drawn at its own origin. */
void sh_pic_draw(const ShPic *p, int x, int y)
{
	if (p->cw < 1)
		return;
	for (int ty = 0; ty < p->ch; ty += 16)
		for (int tx = 0; tx < p->cw; tx += 16) {
			int slot = ktui_sprite_tile_at(p->key, p->cw, tx, ty,
						       NULL, NULL);
			int tw = p->cw - tx, th = p->ch - ty;

			if (slot < 0)
				continue;
			if (tw > 16)
				tw = 16;
			if (th > 16)
				th = 16;
			ktui_draw_sprite(krect(x + tx, y + ty, tw, th), slot,
					 KT_TEXT, KT_SURFACE);
		}
}
