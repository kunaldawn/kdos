/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Pictures out of a terminal view, as pixels the host terminal draws
 *
 * A `--tty` view writes ANSI to its own stdout, so the only way a picture can
 * be pixels there is for the terminal it is running in to draw them. This asks
 * which protocol that terminal speaks and then hands it the rectangles of the
 * frame that hold pictures.
 *
 * THE EMITTER IS A BACKEND WRAPPER AND NOT A CALL AFTER THE FRAME, because the
 * one thing it has to know is which cells the text layer just painted over —
 * and that is knowable only while `prev` still describes the previous frame.
 * By the time `ktui_draw_flush()` returns, `prev` is the current frame and the
 * damage is gone. So: scan, let the real backend write the text, then put the
 * pictures back on top of the spaces it wrote.
 *
 * THE TEXT LAYER IS THE ERASER, and that is deliberate rather than fought.
 * Every sprite cell flushes as a space or as the fallback mark, so a picture
 * whose window closed, moved or scrolled is removed by the same pass that
 * would have redrawn it. Nothing else has to track disappearance.
 *
 * AN ANIMATION IS INVISIBLE IN THE CELLS. A sprite cell names a slot, not a
 * picture, so the next frame of a moving picture writes byte-identical cells
 * and the diff finds nothing. `KtuiSprite.gen` is the only signal, which is
 * what it exists for.
 *
 * A FAILURE HERE FALLS BACK TO CHARACTERS, NEVER TO BLANK CELLS. That is the
 * whole lesson of the attempt this replaces: a picture protocol that puts
 * nothing on the screen is worse than the characters it replaced. Every path
 * that cannot emit leaves the cells the text layer wrote, and `present()`
 * makes those the session's own fallback mark.
 * ---------------------------------
 */

/* KDOS_VIEW_TTYPIX alone: the pixel-path derive lives in main.c, so this file
 * cannot see it. The flag is only ever passed together with the rest. */
#ifdef KDOS_VIEW_TTYPIX

#define _POSIX_C_SOURCE 200809L
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <pixman.h>
#include <sixel.h>

#include "ktui.h"
#include "view.h"

/* Row runs collected before they are merged into rectangles. A tile is at most
 * sixteen cells tall, so an unoccluded one is sixteen runs and one rectangle. */
#define KPIX_RUNS 48

/* Rectangles a picture may be broken into before this gives up on it for the
 * frame. A pointer parked inside a tile already costs four; a window edge
 * across it costs more. Giving up leaves the fallback marks, which is a
 * picture that looks like a picture — dropping the extra rectangles would
 * leave holes of spaces, which is what "nothing appeared" looked like. */
#define KPIX_RECTS 8

/* Never 0: kitty reads `a=d,i=0` as "every picture in this terminal". */
#define KPIX_ID_BASE 0x4B440000u

/* A tile is at most 16x16 cells, so this bounds one encode. */
#define KPIX_MAX_PX (16 * 64)

/* What one rectangle's payload may grow to. kdos-term drops a body over its
 * own cap in silence (there is no reply to a picture), so a rectangle that
 * would exceed a conservative bound is not sent at all and keeps its marks. */
#define KPIX_MAX_BYTES (768u * 1024u)

enum { KPIX_NONE = 0, KPIX_KITTY, KPIX_SIXEL };

typedef struct {
	int x, y, w, h;		/* where on the grid, in cells       */
	int sx, sy;		/* which cell of the sprite is at x,y */
} Rect;

typedef struct {
	int live;		/* a picture occupies cells this frame */
	int nrun, nrect;
	int placed;		/* rectangles actually on the terminal */
	uint64_t key;
	unsigned long gen;
	Rect run[KPIX_RUNS];
	Rect rect[KPIX_RECTS];
	unsigned long long last_ms;
	int dirty;
} Pic;

static Pic st[KTUI_MAX_SPRITES];
static const KtuiBackend *base;
static KtuiBackend wrap;
static int proto;
static int pix_w = 8, pix_h = 16;
static int gate_ms = 50;
static int ptr_x = -1, ptr_y = -1;
static int caret_x = -1, caret_y = -1;
static int full_next;
static sixel_output_t *sx_out;
static sixel_dither_t *sx_dither;

static unsigned long long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000ull +
	       (unsigned long long)(ts.tv_nsec / 1000000);
}

/* ── the probe ──────────────────────────────────────────────────────────── */

/*
 * ONE BURST, ONE READ, AND EVERY BYTE CONSUMED.
 *
 * A reply left in the input queue is not silence — `ktui_input` decodes it as
 * keys and types the protocol text into the session being viewed. So the read
 * runs to the DA1 fence and swallows what it finds.
 *
 * DA1 IS THE FENCE, not the evidence. Every terminal answers it and replies
 * come back in order on one pty, so a CSI reply ending in `c` proves the
 * picture questions have been answered or never will be.
 *
 * THE APC IS TERMINATED WITH `ESC \` AND NEVER WITH BEL: inside an APC string
 * a BEL is an ignored control byte, so a BEL-terminated query is never
 * dispatched and gets no reply at all.
 */
static int read_replies(char *buf, size_t cap)
{
	size_t n = 0;
	unsigned long long deadline = now_ms() + 250;

	while (n + 1 < cap) {
		struct pollfd p = { .fd = 0, .events = POLLIN };
		unsigned long long t = now_ms();
		int left = t >= deadline ? 0 : (int)(deadline - t);
		char c;

		if (left <= 0 || poll(&p, 1, left < 40 ? left : 40) <= 0)
			break;
		if (read(0, &c, 1) != 1)
			break;
		buf[n++] = c;
		buf[n] = '\0';
		/*
		 * The DA1 reply is `CSI ? … c` and it was sent last, so a `c`
		 * closing one is the fence. Matched against the LAST `CSI ?`
		 * in the buffer rather than against the first `c` seen: a
		 * refusal from the kitty query is free text and may contain
		 * one.
		 */
		if (c == 'c') {
			const char *p2 = buf, *last = NULL;

			while ((p2 = strstr(p2, "\033[?")) != NULL) {
				last = p2;
				p2 += 3;
			}
			if (last && (size_t)(last - buf) + 3 <= n)
				break;
		}
	}
	buf[n] = '\0';
	return (int)n;
}

/* `CSI 6;<h>;<w>t` — a cell in pixels, height first. Clamped because a
 * terminal that answers nonsense must not make every picture nonsense. */
static void parse_cell(const char *b)
{
	const char *p = b;

	while ((p = strstr(p, "\033[6;")) != NULL) {
		int h = 0, w = 0;

		if (sscanf(p + 4, "%d;%dt", &h, &w) == 2 && w >= 4 && w <= 64 &&
		    h >= 4 && h <= 64) {
			pix_w = w;
			pix_h = h;
			return;
		}
		p += 4;
	}
}

/* DA1's parameter 4 is the sixel CLAIM. It is a claim about a parser and not
 * about a decoder: this tree's own terminals answer it with pictures compiled
 * out, which is exactly why it is never believed on its own. */
static int da1_has_sixel(const char *b)
{
	const char *p = strstr(b, "\033[?");

	while (p) {
		const char *q = p + 3;

		while (*q && *q != 'c') {
			int v = 0;

			if (*q < '0' || *q > '9') {
				q++;
				continue;
			}
			while (*q >= '0' && *q <= '9')
				v = v * 10 + (*q++ - '0');
			if (v == 4)
				return 1;
		}
		if (*q != 'c')
			return 0;
		p = strstr(q, "\033[?");
	}
	return 0;
}

/* `CSI ?2;0;<w>;<h>S` is a real sixel geometry; `CSI ?2;3;S` is a refusal.
 * Only a terminal that answers the first will actually draw one. */
static int xtsm_has_geometry(const char *b)
{
	const char *p = strstr(b, "\033[?2;");

	if (!p)
		return 0;
	return p[5] == '0';
}

static int by_env(void)
{
	const char *t = getenv("TERM");

	if (getenv("KITTY_WINDOW_ID"))
		return KPIX_KITTY;
	if (!t || !*t)
		return KPIX_NONE;
	if (strstr(t, "kitty") || strstr(t, "ghostty") || strstr(t, "wezterm"))
		return KPIX_KITTY;
	if (strstr(t, "foot") || strstr(t, "mlterm") || strstr(t, "contour") ||
	    strstr(t, "mintty"))
		return KPIX_SIXEL;
	return KPIX_NONE;
}

int view_ttypix_probe(int *cw, int *ch)
{
	static const char burst[] =
		"\033_Gi=1929,s=1,v=1,a=q,t=d,f=24;AAAA\033\\"
		"\033[?2;1S"
		"\033[16t"
		"\033[c";
	const char *force = getenv("KDOS_VIEW_PIX");
	const char *cell = getenv("KDOS_VIEW_CELL");
	const char *ms = getenv("KDOS_VIEW_PIX_MS");
	char buf[1024];
	int silent;

	proto = KPIX_NONE;
	if (ms && *ms) {
		int v = atoi(ms);

		if (v >= 0 && v <= 10000)
			gate_ms = v;
	}
	if (force && !strcmp(force, "off"))
		return KPIX_NONE;

	/* A Linux console has no picture protocol and no DA1 worth waiting
	 * for — the same skip the keyboard protocol push makes. */
	if (ktui_caps & KT_CAP_LINUXVT)
		return KPIX_NONE;

	ktui_term_write(burst, sizeof(burst) - 1);
	ktui_term_flush();
	silent = read_replies(buf, sizeof(buf)) == 0;
	parse_cell(buf);

	if (force && !strcmp(force, "kitty"))
		proto = KPIX_KITTY;
	else if (force && !strcmp(force, "sixel"))
		proto = KPIX_SIXEL;
	else if (strstr(buf, "\033_Gi=1929;OK"))
		proto = KPIX_KITTY;
	else if (strstr(buf, "\033_Gi=1929;E"))
		proto = KPIX_NONE;	/* said no on purpose */
	else if (da1_has_sixel(buf) && xtsm_has_geometry(buf))
		proto = KPIX_SIXEL;
	else if (silent)
		proto = by_env();	/* only when nothing answered at all */

	if (cell && sscanf(cell, "%dx%d", &pix_w, &pix_h) != 2) {
		pix_w = 8;
		pix_h = 16;
	}
	if (pix_w < 4 || pix_w > 64 || pix_h < 4 || pix_h > 64) {
		pix_w = 8;
		pix_h = 16;
	}
	*cw = pix_w;
	*ch = pix_h;
	return proto;
}

/* ── the pixels of one rectangle ────────────────────────────────────────── */

/*
 * A CROP OUT OF THE SPRITE, as RGB888 with alpha composited onto the theme's
 * background. Neither protocol here carries alpha, and a halo round everything
 * transparent reads as a broken decoder.
 *
 * pixman's a8r8g8b8 is a NATIVE-ENDIAN 32-bit word, so the channels come out
 * of the integer and never out of the bytes.
 *
 * The height is rounded up to a multiple of six for the sixel encoder: a band
 * is six pixel rows and a short last band is padded with the background, which
 * would otherwise be painted into the text row below.
 */
static uint8_t *crop_rgb(const KtuiSprite *s, const Rect *r, int *pw, int *ph,
			 int pad6)
{
	pixman_image_t *img = (pixman_image_t *)s->pix;
	const uint32_t *src;
	uint32_t bg;
	int stride, sw, sh, w, h, hh;
	uint8_t *out;

	if (!img)
		return NULL;
	src = pixman_image_get_data(img);
	sw = pixman_image_get_width(img);
	sh = pixman_image_get_height(img);
	stride = pixman_image_get_stride(img) / 4;
	if (!src || sw <= 0 || sh <= 0)
		return NULL;

	w = r->w * pix_w;
	h = r->h * pix_h;
	if (r->sx * pix_w + w > sw)
		w = sw - r->sx * pix_w;
	if (r->sy * pix_h + h > sh)
		h = sh - r->sy * pix_h;
	if (w <= 0 || h <= 0 || w > KPIX_MAX_PX || h > KPIX_MAX_PX)
		return NULL;

	hh = pad6 ? (h + 5) / 6 * 6 : h;
	out = malloc((size_t)w * (size_t)hh * 3);
	if (!out)
		return NULL;

	bg = ((uint32_t)ktui_theme->slot[KT_BG].r << 16) |
	     ((uint32_t)ktui_theme->slot[KT_BG].g << 8) |
	     (uint32_t)ktui_theme->slot[KT_BG].b;
	for (int y = 0; y < hh; y++)
		for (int x = 0; x < w; x++) {
			uint32_t p = y < h
				? src[(size_t)(r->sy * pix_h + y) * stride +
				      (size_t)(r->sx * pix_w + x)]
				: 0;
			unsigned a = (p >> 24) & 0xff;
			unsigned rr = (p >> 16) & 0xff;
			unsigned gg = (p >> 8) & 0xff;
			unsigned bb = p & 0xff;
			size_t o = ((size_t)y * (size_t)w + (size_t)x) * 3;

			if (y >= h || a == 0) {
				rr = (bg >> 16) & 0xff;
				gg = (bg >> 8) & 0xff;
				bb = bg & 0xff;
			} else if (a < 255) {
				/* Un-premultiply, then composite. */
				rr = rr * 255 / a;
				gg = gg * 255 / a;
				bb = bb * 255 / a;
				if (rr > 255)
					rr = 255;
				if (gg > 255)
					gg = 255;
				if (bb > 255)
					bb = 255;
				rr = (rr * a + ((bg >> 16) & 0xff) *
				      (255 - a)) / 255;
				gg = (gg * a + ((bg >> 8) & 0xff) *
				      (255 - a)) / 255;
				bb = (bb * a + (bg & 0xff) * (255 - a)) / 255;
			}
			out[o + 0] = (uint8_t)rr;
			out[o + 1] = (uint8_t)gg;
			out[o + 2] = (uint8_t)bb;
		}
	*pw = w;
	*ph = hh;
	return out;
}

/* ── kitty ──────────────────────────────────────────────────────────────── */

static const char b64tab[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*
 * BASE64 CHUNKS ARE CUT ON THREE-BYTE INPUT BOUNDARIES, so padding can appear
 * only in the last one. A receiver that concatenates the chunks and then stops
 * at the first `=` truncates the picture in silence otherwise.
 */
/*
 * One rectangle, transmitted and placed in one action. `q=2` suppresses the
 * terminal's reply — an unsuppressed OK is decoded by the input layer as
 * keystrokes and typed into the session being viewed. `C=1` keeps the cursor
 * where it was for a terminal that honours it; the bottom row is never used,
 * for terminals that do not.
 */
static void emit_kitty(int vs, int ri, const Rect *r, const KtuiSprite *s)
{
	int pw = 0, ph = 0;
	uint8_t *rgb = crop_rgb(s, r, &pw, &ph, 0);
	size_t n, done = 0;
	unsigned id = KPIX_ID_BASE + (unsigned)vs * KPIX_RECTS + (unsigned)ri;
	char hdr[160];
	int first = 1;

	if (!rgb)
		return;
	n = (size_t)pw * (size_t)ph * 3;
	if (n / 3 * 4 > KPIX_MAX_BYTES) {
		free(rgb);
		return;
	}

	snprintf(hdr, sizeof(hdr), "\033[%d;%dH", r->y + 1, r->x + 1);
	ktui_term_write(hdr, strlen(hdr));

	while (done < n) {
		size_t take = n - done > 3072 ? 3072 : n - done;
		int more = done + take < n;
		char b[4096 + 8];
		size_t o = 0;

		for (size_t i = 0; i < take; i += 3) {
			unsigned v = (unsigned)rgb[done + i] << 16;
			size_t rem = take - i;

			if (rem > 1)
				v |= (unsigned)rgb[done + i + 1] << 8;
			if (rem > 2)
				v |= rgb[done + i + 2];
			b[o++] = b64tab[(v >> 18) & 63];
			b[o++] = b64tab[(v >> 12) & 63];
			b[o++] = rem > 1 ? b64tab[(v >> 6) & 63] : '=';
			b[o++] = rem > 2 ? b64tab[v & 63] : '=';
		}

		if (first) {
			char h2[192];
			int hn = snprintf(h2, sizeof(h2),
					  "\033_Ga=T,q=2,i=%u,f=24,t=d,"
					  "s=%d,v=%d,c=%d,r=%d,C=1%s;",
					  id, pw, ph, r->w, r->h,
					  more ? ",m=1" : "");

			ktui_term_write(h2, (size_t)hn);
			first = 0;
		} else {
			const char *h2 = more ? "\033_Gm=1;" : "\033_Gm=0;";

			ktui_term_write(h2, strlen(h2));
		}
		ktui_term_write(b, o);
		ktui_term_write("\033\\", 2);
		done += take;
	}
	free(rgb);
}

static void kitty_delete(int vs, int ri)
{
	char b[64];
	unsigned id = KPIX_ID_BASE + (unsigned)vs * KPIX_RECTS + (unsigned)ri;
	int n = snprintf(b, sizeof(b), "\033_Ga=d,q=2,d=i,i=%u\033\\", id);

	ktui_term_write(b, (size_t)n);
}

/* ── sixel ──────────────────────────────────────────────────────────────── */

static int sx_write(char *data, int size, void *priv)
{
	(void)priv;
	ktui_term_write(data, (size_t)size);
	return size;
}

/*
 * A FIXED PALETTE AND NO DIFFUSION, because the encoding unit is a TILE. An
 * adaptive palette differs between the tiles of one picture and shows as seams
 * down every tile boundary, and recomputing it per frame makes an animation
 * shimmer. Photographs band; that is the trade a picture assembled from
 * 16x16-cell pieces has to make.
 */
static int sixel_setup(void)
{
	if (sx_out)
		return 0;
	if (SIXEL_FAILED(sixel_output_new(&sx_out, sx_write, NULL, NULL)))
		return -1;
	sixel_output_set_encode_policy(sx_out, SIXEL_ENCODEPOLICY_FAST);
	sx_dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);
	if (!sx_dither) {
		sixel_output_unref(sx_out);
		sx_out = NULL;
		return -1;
	}
	sixel_dither_set_pixelformat(sx_dither, SIXEL_PIXELFORMAT_RGB888);
	sixel_dither_set_diffusion_type(sx_dither, SIXEL_DIFFUSE_NONE);
	return 0;
}

static void emit_sixel(const Rect *r, const KtuiSprite *s)
{
	int pw = 0, ph = 0;
	uint8_t *rgb;
	char hdr[64];

	if (sixel_setup() != 0)
		return;
	rgb = crop_rgb(s, r, &pw, &ph, 1);
	if (!rgb)
		return;

	snprintf(hdr, sizeof(hdr), "\033[%d;%dH", r->y + 1, r->x + 1);
	ktui_term_write(hdr, strlen(hdr));
	sixel_encode(rgb, pw, ph, 3, sx_dither, sx_out);
	free(rgb);
}

/* ── the scan ───────────────────────────────────────────────────────────── */

/* A run is a horizontal span of one sprite on one row whose cell coordinates
 * advance with the screen's. Runs are collected first and merged after, because
 * a run's width is not known until its row ends — merging as they arrive can
 * only ever join single-column spans, and a picture wider than that then needs
 * a rectangle per row and is dropped for want of room. */
static int add_run(Pic *pc, int x, int y, int sx, int sy)
{
	if (pc->nrun > 0) {
		Rect *r = &pc->run[pc->nrun - 1];

		if (r->y == y && r->x + r->w == x && r->sy == sy &&
		    r->sx + r->w == sx) {
			r->w++;
			return 0;
		}
	}
	if (pc->nrun >= KPIX_RUNS)
		return -1;
	pc->run[pc->nrun++] = (Rect){ x, y, 1, 1, sx, sy };
	return 0;
}

/*
 * Rows into rectangles: a run joins the rectangle directly above it when the
 * two describe the same columns of the same sprite. Returns -1 when the result
 * needs more rectangles than there is room for, which leaves the picture to the
 * text layer for this frame rather than drawing part of it.
 */
static int merge_runs(Pic *pc)
{
	pc->nrect = 0;
	for (int i = 0; i < pc->nrun; i++) {
		Rect *n = &pc->run[i];
		int joined = 0;

		for (int j = 0; j < pc->nrect; j++) {
			Rect *r = &pc->rect[j];

			if (r->x == n->x && r->w == n->w && r->sx == n->sx &&
			    r->y + r->h == n->y && r->sy + r->h == n->sy) {
				r->h++;
				joined = 1;
				break;
			}
		}
		if (joined)
			continue;
		if (pc->nrect >= KPIX_RECTS)
			return -1;
		pc->rect[pc->nrect++] = *n;
	}
	return 0;
}

/*
 * ONE PASS OVER THE FRAME WHILE `prev` IS STILL THE PREVIOUS ONE. Everything
 * the emitter needs to know is here: which slots are on screen, where, and
 * whether the text layer is about to paint over them.
 *
 * The LAST ROW IS NEVER PICTURE. A sixel at the bottom margin scrolls in every
 * terminal, and a scroll silently invalidates the whole diff — nothing could
 * detect it afterwards. Those cells keep their fallback mark.
 */
static void scan(const KtuiCell *cur, const KtuiCell *prev, int w, int h,
		 int full)
{
	int over = 0;

	for (int i = 0; i < KTUI_MAX_SPRITES; i++) {
		st[i].live = 0;
		st[i].nrun = 0;
		st[i].nrect = 0;
		st[i].dirty = 0;
	}

	for (int y = 0; y < h - 1; y++)
		for (int x = 0; x < w; x++) {
			int i = y * w + x;
			const KtuiCell *c = &cur[i];
			const KtuiSprite *s;
			int vs;
			Pic *pc;

			if (!KTUI_IS_SPRITE(c->ch))
				continue;
			vs = (int)KTUI_SPRITE_SLOT(c->ch);
			if (vs < 0 || vs >= KTUI_MAX_SPRITES)
				continue;
			s = ktui_sprite_get(vs);
			if (!s || !s->pix)
				continue;	/* a mark, not a picture */
			if (x == ptr_x && y == ptr_y)
				continue;	/* the pointer owns the cell */

			pc = &st[vs];
			pc->live = 1;
			if (add_run(pc, x, y, (int)KTUI_SPRITE_SX(c->ch),
				    (int)KTUI_SPRITE_SY(c->ch)) != 0) {
				over = 1;
				pc->live = 0;	/* too broken up to draw */
				continue;
			}
			if (full || c->ch != prev[i].ch ||
			    c->fg != prev[i].fg || c->bg != prev[i].bg ||
			    c->attr != prev[i].attr)
				pc->dirty = 1;
		}
	(void)over;

	for (int vs = 0; vs < KTUI_MAX_SPRITES; vs++) {
		Pic *pc = &st[vs];
		const KtuiSprite *s;

		if (!pc->live)
			continue;
		if (merge_runs(pc) != 0) {
			pc->live = 0;	/* too broken up to draw */
			continue;
		}
		s = ktui_sprite_get(vs);
		if (!s)
			continue;
		/* The pixels changed and the cells did not — the only signal
		 * an animation gives. */
		if (s->gen != pc->gen || s->key != pc->key)
			pc->dirty = 1;
	}
}

/* ── the flush ──────────────────────────────────────────────────────────── */

static void pix_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int full)
{
	static Pic was[KTUI_MAX_SPRITES];
	unsigned long long t;
	char cup[64];
	int n;

	if (full_next) {
		full = 1;
		full_next = 0;
	}
	scan(cur, prev, w, h, full);

	/* The text layer writes the frame, and every sprite cell in it becomes
	 * a space or the fallback mark. That is the erase. */
	base->flush(cur, prev, w, h, full);

	t = now_ms();
	for (int vs = 0; vs < KTUI_MAX_SPRITES; vs++) {
		Pic *pc = &st[vs];
		Pic *old = &was[vs];
		const KtuiSprite *s;
		int moved;

		if (!old->placed)
			continue;
		moved = !pc->live || pc->nrect != old->nrect ||
			memcmp(pc->rect, old->rect,
			       sizeof(Rect) * (size_t)old->nrect) != 0;
		if (!moved)
			continue;
		/* Kitty draws above the text, so the erase above did not
		 * remove it: every placement of a picture that moved or went
		 * away is deleted by id. Sixel needs nothing — it is pixels in
		 * the cells the text layer just rewrote. */
		if (proto == KPIX_KITTY)
			for (int ri = 0; ri < old->placed; ri++)
				kitty_delete(vs, ri);
		old->placed = 0;
		if (pc->live)
			pc->dirty = 1;
		s = ktui_sprite_get(vs);
		(void)s;
	}

	for (int vs = 0; vs < KTUI_MAX_SPRITES; vs++) {
		Pic *pc = &st[vs];
		const KtuiSprite *s;

		if (!pc->live || !pc->dirty)
			continue;
		s = ktui_sprite_get(vs);
		if (!s || !s->pix)
			continue;
		/* Repaint damage is repaired at once; a picture whose only
		 * change is its pixels is rate-limited, because a session can
		 * send frames faster than a terminal can swallow them. */
		if (s->gen != pc->gen && pc->key == s->key &&
		    was[vs].placed == pc->nrect &&
		    t - pc->last_ms < (unsigned long long)gate_ms)
			continue;

		for (int ri = 0; ri < pc->nrect; ri++) {
			if (proto == KPIX_KITTY)
				emit_kitty(vs, ri, &pc->rect[ri], s);
			else
				emit_sixel(&pc->rect[ri], s);
		}
		/* Recorded at EMIT time, never at scan time: a slot whose
		 * emit was skipped must stay dirty. */
		pc->gen = s->gen;
		pc->key = s->key;
		pc->last_ms = t;
		pc->placed = pc->nrect;
		was[vs] = *pc;
	}
	for (int vs = 0; vs < KTUI_MAX_SPRITES; vs++)
		if (!st[vs].live)
			was[vs].live = 0;

	/* The caret last, because everything above moved it. `ktui_term_caret`
	 * dedupes against where it believes the caret is, so writing the
	 * sequence here keeps that belief true instead of contradicting it. */
	if (caret_x >= 0 && caret_y >= 0)
		n = snprintf(cup, sizeof(cup), "\033[%d;%dH\033[?25h",
			     caret_y + 1, caret_x + 1);
	else
		n = snprintf(cup, sizeof(cup), "\033[?25l");
	ktui_term_write(cup, (size_t)n);
	ktui_term_flush();

	/* A dropped write can cut an escape in half. Forget what is on the
	 * terminal and repaint everything next frame. */
	if (ktui_term_flush_dropped()) {
		for (int vs = 0; vs < KTUI_MAX_SPRITES; vs++)
			was[vs].placed = 0;
		full_next = 1;
		ktui_draw_invalidate();
	}
}

const KtuiBackend *view_ttypix_install(const KtuiBackend *b)
{
	base = b;
	wrap.name = "ttypix";
	wrap.flush = pix_flush;
	wrap.poll_event = b->poll_event;
	wrap.size = b->size;
	wrap.caps = b->caps;
	full_next = 1;
	return &wrap;
}

void view_ttypix_pointer(int x, int y)
{
	ptr_x = x;
	ptr_y = y;
}

void view_ttypix_caret(int x, int y)
{
	caret_x = x;
	caret_y = y;
}

void view_ttypix_forget(int vs)
{
	if (vs < 0 || vs >= KTUI_MAX_SPRITES)
		return;
	if (proto == KPIX_KITTY)
		for (int ri = 0; ri < st[vs].placed; ri++)
			kitty_delete(vs, ri);
	memset(&st[vs], 0, sizeof(st[vs]));
}

void view_ttypix_shutdown(void)
{
	if (proto == KPIX_KITTY)
		for (int vs = 0; vs < KTUI_MAX_SPRITES; vs++)
			for (int ri = 0; ri < st[vs].placed; ri++)
				kitty_delete(vs, ri);
	ktui_term_flush();
	if (sx_dither)
		sixel_dither_unref(sx_dither);
	if (sx_out)
		sixel_output_unref(sx_out);
	sx_dither = NULL;
	sx_out = NULL;
}

#endif /* KDOS_VIEW_TTYPIX */
