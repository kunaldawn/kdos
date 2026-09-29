/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   paintcheck — the partial paint and the cell damage against the full ones
 *
 * libkwl repaints a buffer in part and damages only what changed: over a
 * backdrop, a buffer keeps the picture it wears and has each changed cell —
 * and every cell over a pixel the picture moved since that buffer was
 * painted — laid back on its band of the cached picture first, and libkchrome
 * re-rasterises that cache only where its op list changed. All of it fails
 * SILENTLY — a wrong answer is stale pixels on a screen, never a crash — so
 * it is checked against the thing it must equal, frame by frame.
 *
 * kwl.c is INCLUDED, so flush_commit() is the real one with its real state,
 * and the Wayland wire is replaced underneath it: the generated protocol
 * stubs all reach libwayland-client through wl_proxy_marshal_flags(), which
 * this file defines, so the executable's definition is the one they call. A
 * simulated compositor keeps its own copy of the screen and copies into it
 * exactly the damage each commit names — which is what a compositor with a
 * texture does — and after every commit:
 *
 *   - the screen must equal the buffer just attached (the damage covered
 *     every pixel that changed);
 *   - with an opaque region claimed, every pixel of that buffer must be
 *     opaque (the claim is true);
 *   - the surface size the compositor derives — the viewport destination
 *     where one is set, else the buffer over its scale — must be the
 *     logical size the surface was configured to, whole or fractional.
 *
 * Every commit's buffer is hashed to stdout. selftest.sh runs the same
 * scripted sequence again under KDOS_PAINT_FULL=1, which paints and damages
 * every commit in full, and the two outputs must be identical: the partial
 * paint produced the same pixels as the full one, every frame.
 *
 * The script is a seeded walk over what a surface does: a hover that moves a
 * plate, two translucent plates that swap which is on top, a plate at any
 * pixel position (over double-width glyphs too), plates dropped from the
 * middle of the list, a plate on the rule or past the last cell (which no cell
 * covers, so that commit must be painted in full), display text two rows
 * high whose figure changes (on the backdrop's own slot, on a plate of another
 * slot, centred), a clock tick, a caret, an
 * edit in text with double-width glyphs, an adaptive alpha, a retint, night
 * light, a backdrop replaced (a popup opening, a flat body), no backdrop at
 * all, a resize,
 * a scale change — to a whole number, or to a fraction, where the buffer is
 * device pixels behind a viewport — a spoiled cell, the list scrolled by a line, back, and by a
 * page (with shade characters, whose pattern is anchored to the buffer, and
 * plates that travel with their rows), and the compositor holding a buffer
 * so the same one is painted twice running. A `scroll` run ends each walk
 * with the list scrolled line by line under an op in the remainder past the
 * last cell, over a bare body in KT_BG, which leaves that remainder to the
 * picture: a band moved with its remainder keeps the old row's picture there;
 * then over a flat body with display text changing on the top rows, whose
 * rows are repainted while the band below them moves. The whole walk runs at two font
 * sizes, one cell height even and one odd, since only an odd one puts a
 * moved shade out of phase. A run in which no commit repainted a moved
 * picture in part fails: it would have tested none of that path; so does a
 * `scroll` run in which no paint moved a band, or none moved one over a
 * backdrop.
 *
 * Under KDOS_INSPECT=1 the same walk runs with the developer overlay on. Its
 * tint is timed by the clock, so the hashes are not compared with anything;
 * what is asked is that every commit still leaves the screen equal to the
 * buffer — the overlay's pixels move where no cell did — and, since every
 * inspected commit is whole, no partial commit is looked for.
 *
 * A `glide` run declares the list as a gliding view (kwl_list_view) on every
 * frame, drives the animation clock by hand, and puts the library's own glide
 * frames (glide_refresh) between the surface's: the walk then checks that a
 * glide's pixels are damaged and that the partial paint under a glide is the
 * full one. Each walk ends with a list alone on the surface, scrolled by
 * lines, by several, back, and by more than it holds, and there every commit's
 * list must be the list's own picture at the position the glide presented —
 * the rows at `top` moved by the lag, the rows it opens being the ones the
 * screen showed — painted independently from the cells. A glide run in which
 * no commit presented a list in between tested nothing of it.
 *
 * usage: paintcheck <seed> <steps> [cells|scroll|glide]
 *   `cells` weights the walk toward cell-only changes, which is the partial
 *   path; `scroll` toward scrolling, which is the moved band; `glide` toward
 *   scrolling a gliding list; the default weighting is mostly plates, which is
 *   the full one.
 * ---------------------------------
 */
#include "kwl.c"
/* Included rather than linked, so the harness can read where each glide is. */
#include "kwl_glide.c"

#include <stdarg.h>

#include "kchrome.h"

/* ── the wire ──────────────────────────────────────────────────────────── */

static char fake_surface, fake_shm, fake_comp, fake_viewport;
/* What the compositor holds: the buffer scale and the viewport destination
 * last sent, which together say how large the surface is. */
static int buf_scale = 1, dest_w = -1, dest_h = -1;
/* Commits on the fractional path, which a run must reach. */
static int frac_commits;

static struct wl_buffer *att_buf;
static int ndmg, fails, commits, opaque_on;
static struct {
	int x, y, w, h;
} dmg[4096];

static pixman_image_t *screen;
static long frame_no;
/* Commits whose picture differs from the one before and that were still
 * damaged in part: the moved-picture path, which a run must reach. */
static int moved_part;
static uint64_t last_key;
static int last_valid;

/* XRGB's top byte is not a pixel: pixman may copy it or set it, and a
 * compositor never reads it. */
static uint32_t pixel_mask(pixman_image_t *img)
{
	return pixman_image_get_format(img) == PIXMAN_x8r8g8b8 ? 0x00ffffffu
							       : 0xffffffffu;
}

static uint64_t hash_img(pixman_image_t *img)
{
	const uint32_t *p = pixman_image_get_data(img);
	size_t n = (size_t)pixman_image_get_width(img) *
		   pixman_image_get_height(img);
	uint32_t m = pixel_mask(img);
	uint64_t k = 1469598103934665603ULL;

	for (size_t i = 0; i < n; i++)
		k = (k ^ (p[i] & m)) * 1099511628211ULL;
	return k;
}

static KwlBuffer *buf_of(struct wl_buffer *wl)
{
	for (int i = 0; i < 2; i++)
		if (K.buf[i].wl == wl)
			return &K.buf[i];
	return NULL;
}

static void on_commit(void)
{
	KwlBuffer *b = buf_of(att_buf);

	commits++;
	if (K.frac120)
		frac_commits++;
	if (!b) {
		fprintf(stderr, "paintcheck: a commit with no known buffer\n");
		fails++;
		return;
	}
	/*
	 * A buffer of another size or format is a new texture, which wlroots
	 * uploads whole whatever the damage says (wlr_client_buffer_apply_damage
	 * refuses both). Anything else keeps the old texture outside the damage,
	 * which starts as garbage so a missed rectangle cannot pass by luck.
	 */
	if (!screen || pixman_image_get_width(screen) != b->w ||
	    pixman_image_get_height(screen) != b->h ||
	    pixman_image_get_format(screen) != pixman_image_get_format(b->img)) {
		if (screen)
			pixman_image_unref(screen);
		screen = pixman_image_create_bits(
			pixman_image_get_format(b->img), b->w, b->h, NULL, 0);
		memset(pixman_image_get_data(screen), 0x5a,
		       (size_t)b->w * b->h * 4);
		ndmg = 1;
		dmg[0].x = dmg[0].y = 0;
		dmg[0].w = b->w;
		dmg[0].h = b->h;
	}
	if (b->bd_valid && last_valid && b->bd_key != last_key &&
	    !(ndmg == 1 && dmg[0].w == b->w && dmg[0].h == b->h))
		moved_part++;
	last_key = b->bd_key;
	last_valid = b->bd_valid;
	for (int i = 0; i < ndmg; i++)
		pixman_image_composite32(PIXMAN_OP_SRC, b->img, NULL, screen,
					 dmg[i].x, dmg[i].y, 0, 0, dmg[i].x,
					 dmg[i].y, dmg[i].w, dmg[i].h);

	const uint32_t *s = pixman_image_get_data(screen);
	const uint32_t *p = pixman_image_get_data(b->img);
	uint32_t m = pixel_mask(b->img);
	size_t n = (size_t)b->w * b->h, bad = 0, clear = 0;

	for (size_t i = 0; i < n; i++) {
		if ((s[i] ^ p[i]) & m)
			bad++;
		if (m == 0xffffffffu && (p[i] >> 24) != 0xff)
			clear++;
	}
	if (bad) {
		fprintf(stderr, "paintcheck: commit %ld: %zu pixels changed "
				"outside the damage (%d rectangles)\n",
			frame_no, bad, ndmg);
		fails++;
		pixman_image_composite32(PIXMAN_OP_SRC, b->img, NULL, screen, 0,
					 0, 0, 0, 0, 0, b->w, b->h);
	}
	{
		int sw = dest_w > 0 ? dest_w : b->w / buf_scale;
		int sh = dest_h > 0 ? dest_h : b->h / buf_scale;

		if ((dest_w <= 0 && (b->w % buf_scale || b->h % buf_scale)) ||
		    sw != K.px_w || sh != K.px_h ||
		    (K.frac120 && (dest_w <= 0 || buf_scale != 1))) {
			fprintf(stderr, "paintcheck: commit %ld: a %dx%d buffer "
					"at scale %d, destination %dx%d, is a "
					"%dx%d surface configured %dx%d\n",
				frame_no, b->w, b->h, buf_scale, dest_w, dest_h,
				sw, sh, K.px_w, K.px_h);
			fails++;
		}
	}
	if (opaque_on && clear) {
		fprintf(stderr, "paintcheck: commit %ld: opaque region claimed "
				"over %zu pixels that are not opaque\n",
			frame_no, clear);
		fails++;
	}
	printf("%ld %d %016llx\n", frame_no, (int)(b - K.buf),
	       (unsigned long long)hash_img(b->img));
	ndmg = 0;
	/* The frame callback answers at once; the release pattern in flush()
	 * is what exercises a held buffer. */
	K.frame_cb = NULL;
}

struct wl_proxy *wl_proxy_marshal_flags(struct wl_proxy *proxy, uint32_t op,
					const struct wl_interface *iface,
					uint32_t version, uint32_t flags, ...)
{
	va_list ap;

	(void)version;
	va_start(ap, flags);
	if (proxy == (struct wl_proxy *)&fake_surface) {
		switch (op) {
		case WL_SURFACE_ATTACH:
			att_buf = va_arg(ap, struct wl_buffer *);
			break;
		case WL_SURFACE_DAMAGE_BUFFER:
			if (ndmg < 4096) {
				dmg[ndmg].x = va_arg(ap, int32_t);
				dmg[ndmg].y = va_arg(ap, int32_t);
				dmg[ndmg].w = va_arg(ap, int32_t);
				dmg[ndmg].h = va_arg(ap, int32_t);
				ndmg++;
			}
			break;
		case WL_SURFACE_SET_OPAQUE_REGION:
			opaque_on = va_arg(ap, void *) != NULL;
			break;
		case WL_SURFACE_SET_BUFFER_SCALE:
			buf_scale = va_arg(ap, int32_t);
			break;
		case WL_SURFACE_COMMIT:
			on_commit();
			break;
		}
	}
	if (proxy == (struct wl_proxy *)&fake_viewport &&
	    op == WP_VIEWPORT_SET_DESTINATION) {
		dest_w = va_arg(ap, int32_t);
		dest_h = va_arg(ap, int32_t);
	}
	va_end(ap);
	/* A constructor answers a new object; nothing ever reads it. */
	return iface && !(flags & WL_MARSHAL_FLAG_DESTROY)
		       ? (struct wl_proxy *)calloc(1, 16)
		       : NULL;
}

int wl_proxy_add_listener(struct wl_proxy *p, void (**impl)(void), void *data)
{
	(void)p;
	(void)impl;
	(void)data;
	return 0;
}

uint32_t wl_proxy_get_version(struct wl_proxy *p)
{
	(void)p;
	return 4;
}

void wl_proxy_destroy(struct wl_proxy *p) { (void)p; }

int wl_display_flush(struct wl_display *d)
{
	(void)d;
	return 0;
}

/* ── the surface ───────────────────────────────────────────────────────── */

static uint32_t rng = 1;

static uint32_t rnd(void)
{
	rng ^= rng << 13;
	rng ^= rng >> 17;
	rng ^= rng << 5;
	return rng;
}

static int cols, rows, hover = -1, occluded, clockv, caret, wshift;
static int full_next, cells_mode, scroll_mode, glide_mode, scrollv;
/* The surface stops declaring its list (its page was left). */
static int gl_gone;
/* The animation clock, by hand: a glide's position is a function of it. */
static int64_t fake_now = 1000;
static int64_t fake_clock(void) { return fake_now; }
/* Commits whose paint moved a band, and those of them over a backdrop. */
static unsigned long scrolled, bd_scrolled;
/* Plates that move without a cell moving: two translucent ones that swap
 * which is on top, one at an arbitrary pixel position, the rest plates
 * switched off in the middle of the list, and one outside the grid. */
static int swapv, freex = -1, freey, rest_off, outside;
/* Display text over the list's top rows: 0 none, 1 on the backdrop's own
 * slot, 2 on a band of another slot (a flat plate under it), 3 centred. */
static int dtext;
static KtuiCell *grid;

static uint8_t body_alpha(void) { return occluded ? 255 : 204; }

static void body_draw(pixman_image_t *dst, int w, int h, int scale)
{
	kch_px_body(dst, w, h, scale, body_alpha(), KCH_EDGE_TOP);
	kch_px_replay(dst, scale);
}

static uint64_t body_salt(void) { return body_alpha(); }
static int body_opaque(void) { return body_alpha() == 255; }

/* 0 the taskbar's shape, 1 a popup, 2 a bare stack, 3 a bare stack whose body
 * is KT_BG (the remainder past the last cell is then the picture's, not
 * painted by libkwl), 4 no backdrop at all, 5 a flat KT_BG body (a window's
 * page handed to the backdrop, opaque, the same on every row). */
static void install(int kind)
{
	kcell_reset_slot_alpha();
	if (kind == 4) {
		kwl_set_backdrop(NULL);
		kwl_set_pixels_dirty_fn(NULL);
		return;
	}
	if (kind == 5)
		kch_px_flat(KT_BG);
	else if (kind == 0)
		kch_px_custom(&(KchPxBackdrop){ .draw = body_draw,
						.salt = body_salt,
						.opaque = body_opaque });
	else if (kind == 1)
		kch_px_popup(KT_SURFACE);
	else
		kch_px_bare(kind == 3 ? KT_BG : KT_SURFACE);
	kcell_set_slot_alpha(KT_SURFACE, 0);
}

static void set_cell(int x, int y, uint32_t ch, int fg, int bg, int attr)
{
	if (x < 0 || y < 0 || x >= cols || y >= rows)
		return;

	KtuiCell *c = &grid[(size_t)y * cols + x];

	memset(c, 0, sizeof(*c));
	c->ch = ch;
	c->fg = (uint8_t)fg;
	c->bg = (uint8_t)bg;
	c->attr = (uint16_t)attr;
}

static void text(int x, int y, const char *s, int fg, int bg, int attr)
{
	for (; *s && x < cols; s++, x++)
		set_cell(x, y, (unsigned char)*s, fg, bg, attr);
}

/* The whole surface from its state, the way a surface's draw pass is. */
static void draw(void)
{
	static const char *items[] = { "Settings", "Network", "Audio mixer",
				       "Terminal", "Files", "Display",
				       "Power", "About this machine" };
	char clk[32];

	kch_px_reset();
	for (int y = 0; y < rows; y++)
		for (int x = 0; x < cols; x++)
			set_cell(x, y, ' ', KT_TEXT, KT_SURFACE, 0);
	for (int x = 0; x < cols; x++) {
		set_cell(x, 0, 0x2550, KT_ACCENT, KT_SURFACE, 0);
		set_cell(x, rows - 1, 0x2550, KT_ACCENT, KT_SURFACE, 0);
	}
	snprintf(clk, sizeof(clk), " %02d:%02d:%02d ", clockv / 3600 % 24,
		 clockv / 60 % 60, clockv % 60);
	text(cols - 12, 0, clk, KT_TEXT, KT_SURFACE, KT_A_BOLD);
	for (int y = 1; y < rows - 1; y++) {
		int hl = y == hover;
		/* The line of the list this row shows: everything but the
		 * hover travels with it when the list scrolls. */
		int v = y - 1 + scrollv;
		unsigned uv = (unsigned)v;
		char num[16];

		if (hl)
			kch_px_row(1, y, cols - 2, KCH_T_HOVER);
		text(3, y, items[uv % 8], hl ? KT_ACCENT : KT_TEXT,
		     KT_SURFACE, uv % 5 == 4 ? KT_A_UNDERLINE : 0);
		snprintf(num, sizeof(num), "%5d", v);
		text(cols - 36, y, num, KT_TEXT, KT_SURFACE, 0);
		if (uv % 4 == 1) {
			set_cell(cols - 8, y, 0x4E2D, KT_TEXT, KT_SURFACE, 0);
			set_cell(cols - 7, y, KTUI_WIDE_CONT, KT_TEXT,
				 KT_SURFACE, 0);
		}
		if (uv % 6 == 2)
			text(cols - 20, y, "[opaque]", KT_BG, KT_ACCENT, 0);
		if (uv % 9 == 5)
			for (int k = 0; k < 3; k++)
				set_cell(cols - 24 + k, y, 0x2591 + (unsigned)k,
					 KT_ACCENT, KT_SURFACE, 0);
		if (uv % 7 == 0 && !rest_off)
			kch_px_plate(cols - 30, y, 6, 1, KCH_T_REST, 1);
	}
	{
		/* The same two ops in either order: only their overlap
		 * changes, and only because they blend. */
		int w = kwl_cell_w(), h = kwl_cell_h();
		int ax = 5 * w + 3, ay = h + 4, bx = 7 * w + 1, by = h + 9;

		if (swapv & 1) {
			kch_px_round(bx, by, 4 * w, h, 2,
				     kch_slot_rgb(KT_ACCENT), 0x80);
			kch_px_rect(ax, ay, 4 * w, h, kch_slot_rgb(KT_TEXT),
				    0x60);
		} else {
			kch_px_rect(ax, ay, 4 * w, h, kch_slot_rgb(KT_TEXT),
				    0x60);
			kch_px_round(bx, by, 4 * w, h, 2,
				     kch_slot_rgb(KT_ACCENT), 0x80);
		}
		/* Anywhere, cell-aligned or not, over wide glyphs too. */
		if (freex >= 0)
			kch_px_grad(freex, freey, 3 * w + 7, h + 5, 3,
				    kch_slot_rgb(KT_ACCENT),
				    kch_slot_rgb(KT_SURFACE), 0xC0);
		/* In the remainder past the last cell, or under the rule:
		 * pixels no cell covers. Case 3 sits in the remainder beside
		 * a row of the list, which moves when the list scrolls. */
		if (outside == 1)
			kch_px_rect(cols * w, 2, 5, 6, kch_slot_rgb(KT_TEXT),
				    0xFF);
		else if (outside == 3 && rows >= 8)
			kch_px_rect(cols * w, 5 * h + 2, 5, 3,
				    kch_slot_rgb(KT_TEXT), 0xFF);
		else if (outside == 2)
			kch_px_rect(3, 0, 9, 2, kch_slot_rgb(KT_TEXT), 0xFF);
	}
	if (dtext && rows > 4) {
		char big[16];

		snprintf(big, sizeof(big), "%d%%", clockv % 1000);
		kch_px_text(cols - 14, 1, 9, 2, big, KT_TEXT,
			    dtext == 2 ? KT_ACCENT : KT_SURFACE,
			    dtext == 3 ? KCH_ALIGN_CENTER : KCH_ALIGN_RIGHT);
	}
	if (rows <= 3 && hover > 0)
		kch_px_plate((hover * 8) % (cols - 8), 0, 7, rows, KCH_T_HOVER,
			     1);
	/* Mixed wide and narrow text edited in place: leads and their
	 * continuations move under neighbours that did not change. */
	if (rows > 4) {
		int x = 20, y = rows - 3;

		for (int k = 0; k < 10 && x < cols - 2; k++) {
			unsigned v = (unsigned)(k * 7 + wshift) % 5;

			if (v < 2) {
				set_cell(x, y,
					 0x4E00 + v * 17 + (unsigned)wshift % 3,
					 KT_TEXT, KT_SURFACE, v ? KT_A_BOLD : 0);
				set_cell(x + 1, y, KTUI_WIDE_CONT, KT_TEXT,
					 KT_SURFACE, 0);
				x += 2;
			} else {
				set_cell(x, y, 'a' + v + (unsigned)wshift % 7,
					 KT_TEXT, KT_SURFACE,
					 v == 3 ? KT_A_ITALIC : 0);
				x++;
			}
		}
	}
	if (rows > 3)
		set_cell(14 + caret % 3, rows - 2, caret & 1 ? 0x2588 : ' ',
			 KT_TEXT, KT_SURFACE, KT_A_REVERSE * (caret & 2));
}

static void surf_size(int c, int r, int scale)
{
	cols = c;
	rows = r;
	free(grid);
	grid = calloc((size_t)c * r, sizeof(*grid));
	/* Five pixels the grid does not reach, so the remainder is in play. */
	K.px_w = c * kcell_w() + 5;
	K.px_h = r * kcell_h() + K.rule;
	K.scale = scale;
}

static void flush(void)
{
	int full = full_next;
	unsigned long sc = K.scrolls;

	frame_no++;
	full_next = 0;
	if (glide_mode) {
		/* What kwl_present() does for a surface that declared it. */
		if (!gl_gone)
			kwl_list_view(0, 1, cols, rows - 2, scrollv);
		kwl_glide_frame();
	}
	flush_commit(grid, cols, rows, full);
	if (K.scrolls != sc) {
		scrolled++;
		if (K.backdrop)
			bd_scrolled++;
	}
	/* Usually the other buffer comes back at once; sometimes it is held,
	 * so one buffer is painted twice running or a frame is stashed and
	 * committed by the release. */
	int nb = K.cur_buf;

	if (rnd() % 5)
		buffer_release(&K.buf[nb], K.buf[nb].wl);
	if (rnd() % 7 == 0)
		buffer_release(&K.buf[nb ^ 1], K.buf[nb ^ 1].wl);
}

static void step(void)
{
	unsigned ev = rnd() % 29;

	if (cells_mode && ev < 12 && rnd() % 4)
		ev = 6 + rnd() % 5;
	if ((scroll_mode || glide_mode) && rnd() % 3)
		ev = 23 + rnd() % 4;
	if (glide_mode) {
		/* Time goes by between two draws, and the library commits the
		 * glide's own frames in it. */
		fake_now += rnd() % 40;
		for (unsigned k = rnd() % 3; k; k--) {
			glide_refresh();
			fake_now += 4 + rnd() % 16;
		}
	}
	switch (ev) {
	case 0: case 1: case 2: case 3: case 4:
		hover = 1 + (int)(rnd() % (unsigned)(rows > 2 ? rows - 2 : 1));
		break;
	case 5:
		hover = -1;
		break;
	case 6: case 7: case 8:
		clockv += 1 + (int)(rnd() % 70);
		break;
	case 9: case 10:
		caret++;
		if (rnd() & 1)
			wshift++;
		break;
	case 11:				/* a redescription: no change */
		break;
	case 12:
		occluded ^= 1;
		break;
	case 13: {
		static int t;

		t ^= 1;
		ktui_theme_set(ktui_themes[t % ktui_ntheme].name);
		kch_tone_reset();	/* what a retint does */
		full_next = 1;
		break;
	}
	case 14:
		ktui_theme_night(rnd() & 1);
		kch_tone_reset();
		full_next = 1;
		break;
	case 15:
		/* The rule is the taskbar's and the taskbar always has a
		 * backdrop; with none, the rule's gap is never painted. */
	{
		/* 4 is no backdrop, which the rule never has: there it is the
		 * flat body instead. */
		int k = (int)(rnd() % (K.rule ? 5 : 6));

		install(k == 4 && K.rule ? 5 : k);
		break;
	}
	case 16:
		surf_size(30 + (int)(rnd() % 50), 3 + (int)(rnd() % 30),
			  K.scale);
		break;
	case 17: {
		/* apply_scale() invalidates the whole grid with the scale. A
		 * fraction is scale 1 over device units — the font it would
		 * load at the device size is the harness's business, and the
		 * pixels are counted the same either way. */
		unsigned k = rnd() % 3;

		K.frac120 = k == 2 ? (rnd() & 1 ? 180 : 150) : 0;
		surf_size(cols, rows, k == 1 ? 2 : 1);
		full_next = 1;
		break;
	}
	case 18:
		kwl_owe((int)(rnd() % (unsigned)cols),
			(int)(rnd() % (unsigned)rows), 3, 1);
		break;
	case 19:
		swapv++;
		break;
	case 20:
		freex = (int)(rnd() % (unsigned)(cols * kwl_cell_w())) - 8;
		freey = (int)(rnd() % (unsigned)(rows * kwl_cell_h())) - 4;
		if (rnd() % 3 == 0)
			clockv++;
		break;
	case 21:
		rest_off ^= 1;
		break;
	case 22:
		outside = (int)(rnd() % 4);
		break;
	case 23:				/* a line of output */
		scrollv++;
		break;
	case 24:
		scrollv--;
		break;
	case 25:				/* a page, either way */
		scrollv += (rnd() & 1 ? 1 : -1) *
			   (2 + (int)(rnd() % (unsigned)(rows > 4 ? rows - 3 : 1)));
		break;
	case 26:				/* a line and the clock */
		scrollv++;
		clockv++;
		break;
	case 27:
		dtext = (dtext + 1) % 4;
		break;
	default:
		hover = 1 + (int)(rnd() % (unsigned)(rows > 2 ? rows - 2 : 1));
		clockv++;
		caret++;
		break;
	}
	draw();
	flush();
}

/* ── libkcell's scroll on its own ──────────────────────────────────────── */

static void unit_row(KtuiCell *g, int c, int y, int v)
{
	char t[64];

	snprintf(t, sizeof(t), " line %d of the list %s", v,
		 v % 3 ? "with text" : "");
	for (int x = 0; x < c; x++) {
		KtuiCell *k = &g[(size_t)y * c + x];

		memset(k, 0, sizeof(*k));
		k->ch = x < (int)strlen(t) ? (unsigned char)t[x] : ' ';
		k->fg = KT_TEXT;
		k->bg = v % 4 ? KT_BG : KT_SURFACE;
	}
}

/*
 * A grid painted whole, then the same list scrolled under a fixed first row:
 * the band found must be exactly the one that moved, the move must leave
 * `prev` describing the pixels so the diff that follows finds only the rows
 * the band exposed — plus the row whose source was the half-clipped last one,
 * which the move cannot fill — and the partial paint after it must equal a
 * full paint of the scrolled grid. Up by three, then down by two; then text
 * scrolled under a run of blanks, and a caret that is no scroll at all.
 */
static void scroll_unit(void)
{
	const int c = 34, r = 12, shifts[2] = { 3, -2 };
	int ch = kcell_h(), pw = c * kcell_w(), ph = r * ch - ch / 2;
	KtuiCell *a = calloc((size_t)c * r, sizeof(*a));
	KtuiCell *b = calloc((size_t)c * r, sizeof(*b));
	KtuiCell *prev = calloc((size_t)c * r, sizeof(*prev));
	KCellSpan *sp = calloc((size_t)r, sizeof(*sp));
	pixman_image_t *img = pixman_image_create_bits(PIXMAN_a8r8g8b8, pw, ph,
						       NULL, 0);
	pixman_image_t *ref = pixman_image_create_bits(PIXMAN_a8r8g8b8, pw, ph,
						       NULL, 0);
	int top = 0;
	KCellScroll sc;

	kcell_reset_slot_alpha();
	kcell_set_bg_preserve(false);
	for (int y = 0; y < r; y++)
		unit_row(a, c, y, y == 0 ? -1 : top + y);
	kcell_paint(img, a, prev, c, r, 1, 1, pw, ph);
	for (int t = 0; t < 2; t++) {
		int d = shifts[t], want_y, want_n, want_from, want_rows;

		top += d;
		for (int y = 0; y < r; y++)
			unit_row(b, c, y, y == 0 ? -1 : top + y);
		if (d > 0) {		/* up: rows 1.. come from 1+d.. */
			want_y = 1;
			want_n = r - 1 - d;
			want_from = 1 + d;
			/* the exposed rows, and the one fed by the clipped row */
			want_rows = d + 1;
		} else {		/* down: rows 1-d.. come from 1.. */
			want_y = 1 - d;
			want_n = r - 1 + d;
			want_from = 1;
			want_rows = -d;
		}
		kcell_diff_spans(b, prev, c, r, sp);
		if (!kcell_scroll_find(b, prev, c, r, 1, sp, &sc) ||
		    sc.y != want_y || sc.n != want_n || sc.from != want_from) {
			fprintf(stderr, "paintcheck: scroll %+d: band %d+%d from "
					"%d, want %d+%d from %d\n",
				d, sc.y, sc.n, sc.from, want_y, want_n,
				want_from);
			fails++;
			break;
		}
		if (kcell_scroll_apply(img, prev, c, r, &sc, 1, pw, ph) != 0) {
			fprintf(stderr, "paintcheck: scroll %+d: not moved\n", d);
			fails++;
			break;
		}

		int n = kcell_diff_spans(b, prev, c, r, sp);

		if (n != want_rows) {
			fprintf(stderr, "paintcheck: scroll %+d: %d rows left to "
					"paint after the move, want %d\n",
				d, n, want_rows);
			fails++;
		}
		kcell_paint(img, b, prev, c, r, 0, 1, pw, ph);
		kcell_paint(ref, b, NULL, c, r, 1, 1, pw, ph);
		pixman_image_set_clip_region32(img, NULL);
		pixman_image_set_clip_region32(ref, NULL);
		if (memcmp(pixman_image_get_data(img), pixman_image_get_data(ref),
			   (size_t)pw * ph * 4)) {
			fprintf(stderr, "paintcheck: scroll %+d: moved and "
					"repainted != painted whole\n", d);
			fails++;
		}
		memcpy(a, b, (size_t)c * r * sizeof(*a));
	}
	/*
	 * Blank rows match each other at every shift. Eight of them above three
	 * lines of text that scrolled by one: the band is the two text rows that
	 * moved, not the longer run of blanks, which moving would spare nothing.
	 */
	for (int y = 0; y < r; y++) {
		unit_row(a, c, y, y == 0 ? -1 : 100 + y);
		unit_row(b, c, y, y == 0 ? -1 : 101 + y);
		if (y > 0 && y < r - 3) {
			for (int x = 0; x < c; x++) {
				a[(size_t)y * c + x].ch = ' ';
				b[(size_t)y * c + x].ch = ' ';
				a[(size_t)y * c + x].bg = KT_BG;
				b[(size_t)y * c + x].bg = KT_BG;
			}
		}
	}
	memcpy(prev, a, (size_t)c * r * sizeof(*a));
	kcell_diff_spans(b, prev, c, r, sp);
	if (!kcell_scroll_find(b, prev, c, r, 1, sp, &sc) || sc.y != r - 3 ||
	    sc.n != 2 || sc.from != r - 2) {
		fprintf(stderr, "paintcheck: text under blanks: band %d+%d from "
				"%d, want %d+2 from %d\n",
			sc.y, sc.n, sc.from, r - 3, r - 2);
		fails++;
	}
	/* A caret and a clock are not a scroll. */
	memcpy(b, a, (size_t)c * r * sizeof(*a));
	b[(size_t)3 * c + 5].ch = 0x2588;
	b[(size_t)(r - 1) * c + 9].ch = 'x';
	memcpy(prev, a, (size_t)c * r * sizeof(*a));
	kcell_diff_spans(b, prev, c, r, sp);
	if (kcell_scroll_find(b, prev, c, r, 1, sp, &sc)) {
		fprintf(stderr, "paintcheck: two changed cells taken for a "
				"scroll\n");
		fails++;
	}
	pixman_image_unref(img);
	pixman_image_unref(ref);
	free(a);
	free(b);
	free(prev);
	free(sp);
}

/* ── a gliding list against its own picture ────────────────────────────── */

/* Commits that presented the list in between two positions. */
static long gl_between;

/* The list alone, at `v`: row y is item v + y - 1, and nothing else on the
 * surface moves. No shade characters — their pattern is anchored to the
 * buffer, so a list presented part of a row off is not a list painted at a
 * whole row — and no plates. */
static void gl_draw(KtuiCell *g, int v)
{
	static const char *items[] = { "Settings", "Network", "Audio mixer",
				       "Terminal", "Files", "Display",
				       "Power", "About this machine" };
	KtuiCell *keep = grid;

	grid = g;
	for (int y = 0; y < rows; y++)
		for (int x = 0; x < cols; x++)
			set_cell(x, y, ' ', KT_TEXT, KT_SURFACE, 0);
	text(2, 0, "the list, alone", KT_ACCENT, KT_SURFACE, KT_A_BOLD);
	for (int y = 1; y < rows - 1; y++) {
		unsigned uv = (unsigned)(v + y - 1);
		char num[16];

		text(3, y, items[uv % 8], KT_TEXT, uv % 3 ? KT_SURFACE : KT_BG,
		     uv % 5 == 4 ? KT_A_UNDERLINE : 0);
		snprintf(num, sizeof(num), "%6d", v + y - 1);
		text(cols - 12, y, num, KT_ACCENT, KT_SURFACE, KT_A_BOLD);
		if (uv % 4 == 1) {
			set_cell(cols - 20, y, 0x4E2D, KT_TEXT, KT_SURFACE, 0);
			set_cell(cols - 19, y, KTUI_WIDE_CONT, KT_TEXT,
				 KT_SURFACE, 0);
		}
	}
	text(2, rows - 1, "status", KT_MID, KT_SURFACE, 0);
	grid = keep;
}

/*
 * The buffer just attached, against the list painted from its cells at the
 * position the glide presented: `top` rows down, less the lag. That position
 * spans two whole-row pictures, the one at the row it starts in and the one a
 * row further, and each pixel row of the list is taken from whichever holds
 * it.
 */
static void gl_check(void)
{
	KwlBuffer *b = buf_of(att_buf);
	int scale = K.scale > 0 ? K.scale : 1;
	int cw = kcell_w() * scale, ch = kcell_h() * scale;
	int H = (rows - 2) * ch, W = cols * cw, T = scrollv, L = 0;
	int gh, gs, rs;

	if (!b)
		return;
	for (int i = 0; i < KWL_GLIDES; i++)
		if (G[i].used && G[i].x == 0 && G[i].y == 1) {
			T = G[i].top;
			L = G[i].shown;
		}
	if (L)
		gl_between++;

	int64_t p0 = (int64_t)T * ch - L;
	int a = (int)(p0 >= 0 ? p0 / ch : -((-p0 + ch - 1) / ch));
	int off = (int)(p0 - (int64_t)a * ch);
	KtuiCell *ga = calloc((size_t)cols * rows, sizeof(*ga));
	KtuiCell *gb = calloc((size_t)cols * rows, sizeof(*gb));
	pixman_format_code_t fmt = pixman_image_get_format(b->grid);

	gh = pixman_image_get_height(b->grid);
	pixman_image_t *ra = pixman_image_create_bits(fmt, b->w, gh, NULL, 0);
	pixman_image_t *rb = pixman_image_create_bits(fmt, b->w, gh, NULL, 0);

	gl_draw(ga, a);
	gl_draw(gb, a + 1);
	kcell_paint(ra, ga, NULL, cols, rows, 1, scale, b->w, gh);
	kcell_paint(rb, gb, NULL, cols, rows, 1, scale, b->w, gh);
	pixman_image_set_clip_region32(ra, NULL);
	pixman_image_set_clip_region32(rb, NULL);

	const uint32_t *got = pixman_image_get_data(b->grid);
	const uint32_t *pa = pixman_image_get_data(ra);
	const uint32_t *pb = pixman_image_get_data(rb);
	uint32_t m = pixel_mask(b->grid);
	int bad = 0;

	gs = pixman_image_get_stride(b->grid) / 4;
	rs = pixman_image_get_stride(ra) / 4;
	for (int y = 0; y < H && !bad; y++) {
		int p = off + y;
		const uint32_t *want = p < H ? pa + (size_t)(ch + p) * rs
					     : pb + (size_t)p * rs;
		const uint32_t *row = got + (size_t)(ch + y) * gs;

		for (int x = 0; x < W; x++)
			if ((row[x] ^ want[x]) & m) {
				fprintf(stderr, "paintcheck: commit %ld: the "
						"gliding list at top %d, lag "
						"%d differs from its picture "
						"at pixel %d,%d\n",
					frame_no, T, L, x, y);
				fails++;
				bad = 1;
				break;
			}
	}
	pixman_image_unref(ra);
	pixman_image_unref(rb);
	free(ga);
	free(gb);
}

/* A commit, if one went out, is checked. */
static void gl_flush(void)
{
	int c = commits;

	flush();
	if (commits != c)
		gl_check();
}

static void gl_refresh(void)
{
	int c = commits;

	glide_refresh();
	if (commits != c)
		gl_check();
}

/*
 * By a line, by three, back, by several while one is still under way, and by
 * more than the list holds (which is a jump, and lands at once), with the
 * library's own frames between the draws. Then far past the end: one commit
 * at most puts the list's own picture back, and after it nothing is owed and
 * nothing is committed. Last, a list that stops being declared while it is
 * in between: the next commit, with no cell changed, must put its own
 * picture back and damage it.
 */
static void glide_segment(void)
{
	static const int moves[] = { 1, 1, 3, -2, 0, 5, -1, 0, 3, 3, 3, -9,
				     30, 0, 2, -3, -1, -1, 4 };

	surf_size(60, 20, K.scale);
	install(4);
	scrollv = 40;
	gl_draw(grid, scrollv);
	gl_flush();
	for (size_t i = 0; i < sizeof(moves) / sizeof(moves[0]); i++) {
		scrollv += moves[i];
		fake_now += rnd() % 12;
		gl_draw(grid, scrollv);
		gl_flush();
		for (unsigned k = 1 + rnd() % 3; k; k--) {
			fake_now += 6 + rnd() % 14;
			gl_refresh();
		}
	}
	for (int i = 0; i < 2; i++)
		if (K.buf[i].busy)
			buffer_release(&K.buf[i], K.buf[i].wl);
	fake_now += 1000;
	for (int k = 0; k < 3; k++)
		gl_refresh();
	if (kwl_glide_owed()) {
		fprintf(stderr, "paintcheck: a glide is still owed a commit "
				"long after it landed\n");
		fails++;
	}

	int c = commits;

	gl_refresh();
	if (commits != c) {
		fprintf(stderr, "paintcheck: a glide frame went out with "
				"nothing moving\n");
		fails++;
	}

	for (int i = 0; i < 2; i++)
		if (K.buf[i].busy)
			buffer_release(&K.buf[i], K.buf[i].wl);
	scrollv += 3;
	gl_draw(grid, scrollv);
	gl_flush();
	fake_now += 20;
	gl_gone = 1;
	for (int k = 0; k < 3; k++) {
		gl_flush();
		fake_now += 20;
	}
	gl_gone = 0;
	if (kwl_glide_owed()) {
		fprintf(stderr, "paintcheck: a list that stopped being declared "
				"is still owed a commit\n");
		fails++;
	}
}

/*
 * THE PIXEL UNITS, AND THE PATH A SCALE TAKES, before any walk.
 *
 * On the integer path every conversion is the identity. On a fractional one a
 * size goes in to the nearest device pixel, a position comes out as the
 * logical pixel it lies in and an extent as the one covering its last device
 * pixel — checked for every unit up to 600 at five scales, a pointer at every
 * 37th 256th of a pixel too. Then apply_scale(): a fraction with a viewport
 * loads the font at the device size, scale 1, the device cell never over the
 * named cell times the scale; a whole number is that scale over the font as
 * named; a fraction with no viewport rounds up to the integer path.
 */
static int units_check(void)
{
	static const int S[] = { 150, 180, 210, 144, 90 };
	int n = 0;

	memset(&K, 0, sizeof(K));
	if (to_unit(37) != 37 || from_unit_floor(37) != 37 ||
	    from_unit_ceil(37) != 37 ||
	    ptr_unit(wl_fixed_from_double(7.75)) != 7) {
		fprintf(stderr, "paintcheck: the integer path converts\n");
		n++;
	}
	K.frac120 = 180;
	if (to_unit(100) != 150 || to_unit(3) != 5 || from_unit_floor(149) != 99 ||
	    from_unit_ceil(149) != 100 || from_unit_floor(150) != 100 ||
	    from_unit_ceil(150) != 100 ||
	    ptr_unit(wl_fixed_from_double(10.5)) != 15 ||
	    ptr_unit(wl_fixed_from_double(-0.5)) != -1) {
		fprintf(stderr, "paintcheck: 1.5 converts wrongly\n");
		n++;
	}
	for (unsigned k = 0; k < sizeof(S) / sizeof(S[0]); k++) {
		long long s = S[k];

		K.frac120 = S[k];
		for (int u = 0; u <= 600; u++) {
			long long f = from_unit_floor(u), c = from_unit_ceil(u);
			long long t = to_unit(u);

			if (!(f * s <= u * 120LL && u * 120LL < (f + 1) * s) ||
			    !((c - 1) * s < u * 120LL && u * 120LL <= c * s) ||
			    !(2 * t * 120 - 120 <= 2 * u * s &&
			      2 * u * s <= 2 * t * 120 + 120)) {
				fprintf(stderr, "paintcheck: unit %d at %lld\n",
					u, s);
				n++;
				break;
			}
		}
		for (int v = -600; v <= 600 * 256; v += 37) {
			long long u = ptr_unit((wl_fixed_t)v);

			if (!(u * 120 * 256 <= v * s &&
			      v * s < (u + 1) * 120 * 256)) {
				fprintf(stderr, "paintcheck: pointer %d at %lld\n",
					v, s);
				n++;
				break;
			}
		}
	}

	memset(&K, 0, sizeof(K));
	snprintf(K.font, sizeof(K.font), "%s", "monospace:pixelsize=16");
	if (kcell_font_load(K.font) != 0)
		return n + 1;
	K.lcw = kcell_w();
	K.lch = kcell_h();
	K.on_output = -1;
	K.viewport = (struct wp_viewport *)&fake_viewport;
	K.s120 = 180;
	apply_scale();
	if (K.frac120 != 180 || K.scale != 1 ||
	    kcell_w() * 120 > K.lcw * 180 || kcell_h() * 120 > K.lch * 180 ||
	    kcell_h() <= K.lch) {
		fprintf(stderr, "paintcheck: 1.5 is %d at scale %d, a %dx%d "
				"cell over %dx%d\n", K.frac120, K.scale,
			kcell_w(), kcell_h(), K.lcw, K.lch);
		n++;
	}
	K.s120 = 150;
	apply_scale();
	if (K.frac120 != 150 || K.scale != 1 ||
	    kcell_w() * 120 > K.lcw * 150 || kcell_h() * 120 > K.lch * 150) {
		fprintf(stderr, "paintcheck: 1.25 is %d, a %dx%d cell\n",
			K.frac120, kcell_w(), kcell_h());
		n++;
	}
	K.s120 = 240;
	apply_scale();
	if (K.frac120 || K.scale != 2 || kcell_w() != K.lcw ||
	    kcell_h() != K.lch) {
		fprintf(stderr, "paintcheck: 2 is %d at scale %d, a %dx%d "
				"cell\n", K.frac120, K.scale, kcell_w(),
			kcell_h());
		n++;
	}
	K.viewport = NULL;
	K.s120 = 180;
	apply_scale();
	if (K.frac120 || K.scale != 2) {
		fprintf(stderr, "paintcheck: 1.5 with no viewport is %d at "
				"scale %d\n", K.frac120, K.scale);
		n++;
	}
	K.s120 = 0;
	apply_scale();
	if (K.scale != 1 || kcell_w() != K.lcw) {
		fprintf(stderr, "paintcheck: no scale is %d\n", K.scale);
		n++;
	}
	return n;
}

int main(int argc, char **argv)
{
	int steps = argc > 2 ? atoi(argv[2]) : 300;

	rng = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 0) : 1;
	if (!rng)
		rng = 1;
	cells_mode = argc > 3 && !strcmp(argv[3], "cells");
	scroll_mode = argc > 3 && !strcmp(argv[3], "scroll");
	glide_mode = argc > 3 && !strcmp(argv[3], "glide");
	if (glide_mode) {
		/* The backend with the frame clock, so a glide moves at all,
		 * and the animation clock in this file's hands. */
		ktui_backend_set(&kwl_backend);
		ktui_anim_set_clock(fake_clock);
	}
	/* Two sizes whose cell heights differ in parity, found by asking. */
	int sizes[2] = { 16, 0 }, h16;

	if (kcell_font_load("monospace:pixelsize=16") != 0) {
		fprintf(stderr, "paintcheck: no usable font\n");
		return 2;
	}
	h16 = kcell_h();
	for (int px = 17; px <= 24 && !sizes[1]; px++) {
		char spec[48];

		snprintf(spec, sizeof(spec), "monospace:pixelsize=%d", px);
		if (kcell_font_load(spec) == 0 && (kcell_h() ^ h16) & 1)
			sizes[1] = px;
	}
	if (!sizes[1]) {
		fprintf(stderr, "paintcheck: no font size with an odd/even "
				"cell height beside %d\n", h16);
		return 2;
	}
	fails += units_check();
	/* Each size once with no rule, once with the taskbar's rule above the
	 * grid. */
	for (int run = 0; run < 4; run++) {
		int rule = run & 1 ? 3 : 0;
		char spec[48];

		snprintf(spec, sizeof(spec), "monospace:pixelsize=%d",
			 sizes[run >> 1]);
		if (kcell_font_load(spec) != 0) {
			fprintf(stderr, "paintcheck: font %s\n", spec);
			return 2;
		}
		if (!rule)
			scroll_unit();
		memset(&K, 0, sizeof(K));
		/* A new surface, so the compositor holds no opaque region for
		 * it: a claim the last run's surface left would otherwise be
		 * charged to this one, which never sent it. */
		opaque_on = 0;
		K.configured = 1;
		K.surface = (struct wl_surface *)&fake_surface;
		K.shm = (struct wl_shm *)&fake_shm;
		K.compositor = (struct wl_compositor *)&fake_comp;
		K.viewport = (struct wp_viewport *)&fake_viewport;
		K.dest_w = K.dest_h = dest_w = dest_h = -1;
		buf_scale = 1;
		K.rule = rule;
		K.on_output = -1;
		surf_size(60, 20, 1);
		install(0);
		scrollv = 0;
		draw();
		flush();
		for (int i = 0; i < steps; i++)
			step();
		if (glide_mode)
			glide_segment();
		if (!scroll_mode)
			continue;
		/* The list scrolled under an op in the remainder, over a body
		 * that leaves the remainder to the picture: the moved band
		 * must not carry the remainder with it. */
		surf_size(60, 20, K.scale);
		install(3);
		outside = 3;
		hover = -1;
		draw();
		flush();
		for (int i = 0; i < 12; i++) {
			scrollv++;
			draw();
			flush();
		}
		/* And over a flat body with display text on the list's top
		 * rows: the band below the text moves, the rows it covers are
		 * repainted, and the text changes as it goes. */
		install(5);
		outside = 0;
		dtext = 2;
		draw();
		flush();
		for (int i = 0; i < 12; i++) {
			scrollv++;
			clockv += 7;
			draw();
			flush();
		}
		dtext = 0;
	}
	/* A run that never took the moved-picture path tested nothing of it. */
	/* Under KDOS_INSPECT every commit is whole, so there is no part. */
	if (!paint_full_forced() && !kwl_insp_on() && !moved_part) {
		fprintf(stderr, "paintcheck: no commit repainted a moved "
				"picture in part\n");
		fails++;
	}
	/* A scroll run that never moved a band tested nothing of that path. */
	if (scroll_mode && !paint_full_forced() && !kwl_insp_on() &&
	    (!scrolled || !bd_scrolled)) {
		fprintf(stderr, "paintcheck: %lu paints moved a band, %lu over "
				"a backdrop; a scroll run needs both\n",
			scrolled, bd_scrolled);
		fails++;
	}
	/* A glide run that never presented a list in between tested none of
	 * the glide. */
	if (glide_mode && !gl_between) {
		fprintf(stderr, "paintcheck: no commit presented a gliding "
				"list in between\n");
		fails++;
	}
	/* Nor did one that never drew at a fractional scale. */
	if (!frac_commits) {
		fprintf(stderr, "paintcheck: no commit at a fractional "
				"scale\n");
		fails++;
	}
	fprintf(stderr, "paintcheck: %d commits (%d fractional), %d moved in part, "
			"%lu moved a band (%lu over a backdrop), cell height "
			"%d and %d, %ld glide frames checked in between, "
			"%d failures\n",
		commits, frac_commits, moved_part, scrolled, bd_scrolled, h16,
		kcell_h(),
		gl_between, fails);
	return fails ? 1 : 0;
}
