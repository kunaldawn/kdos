/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   libktui — cell buffer and primitives
 * ---------------------------------
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kbase.h"
#include "ktui.h"

/* Every glyph here was checked against uni/kdos.uni — the 512-glyph charset
 * ter-kdos32n is generated from. A character that is not in that file renders
 * as a blank on the TTY, which is why there is no ▓, no half block, and no
 * ← or → (the console font has ↑ and ↓ but not the horizontal pair; ◀ ▶ are
 * the ones that exist). Do not add a glyph to this table without grepping. */
static const char *glyph_utf8[KT_G_N] = {
	[KT_G_HL] = "─", [KT_G_VL] = "│",
	[KT_G_TL] = "┌", [KT_G_TR] = "┐",
	[KT_G_BL] = "└", [KT_G_BR] = "┘",
	[KT_G_TEE_L] = "├", [KT_G_TEE_R] = "┤",
	[KT_G_TEE_T] = "┬", [KT_G_TEE_B] = "┴",
	[KT_G_CROSS] = "┼",
	[KT_G_DHL] = "═", [KT_G_DVL] = "║",
	[KT_G_DTL] = "╔", [KT_G_DTR] = "╗",
	[KT_G_DBL] = "╚", [KT_G_DBR] = "╝",
	[KT_G_FULL] = "█", [KT_G_SHADE] = "░",
	[KT_G_DOT] = "·", [KT_G_BULLET] = "•", [KT_G_SQUARE] = "■",
	[KT_G_UP] = "↑", [KT_G_DOWN] = "↓",
	[KT_G_LEFT] = "◀", [KT_G_RIGHT] = "▶",
	[KT_G_ELLIPSIS] = "…", [KT_G_DEG] = "°",
};

static const char *glyph_ascii[KT_G_N] = {
	[KT_G_HL] = "-", [KT_G_VL] = "|",
	[KT_G_TL] = "+", [KT_G_TR] = "+", [KT_G_BL] = "+", [KT_G_BR] = "+",
	[KT_G_TEE_L] = "+", [KT_G_TEE_R] = "+", [KT_G_TEE_T] = "+", [KT_G_TEE_B] = "+",
	[KT_G_CROSS] = "+",
	[KT_G_DHL] = "=", [KT_G_DVL] = "|",
	[KT_G_DTL] = "+", [KT_G_DTR] = "+", [KT_G_DBL] = "+", [KT_G_DBR] = "+",
	[KT_G_FULL] = "#", [KT_G_SHADE] = ".",
	[KT_G_DOT] = ".", [KT_G_BULLET] = "*", [KT_G_SQUARE] = "#",
	[KT_G_UP] = "^", [KT_G_DOWN] = "v", [KT_G_LEFT] = "<", [KT_G_RIGHT] = ">",
	[KT_G_ELLIPSIS] = "...", [KT_G_DEG] = "o",
};

const char *ktui_glyph[KT_G_N];
static uint32_t glyph_cp[KT_G_N];

static KtuiCell *front, *back;
static int bw, bh;
static int force_full;
static int offscreen;
static int ptr_x = -1, ptr_y = -1;

/* A clip krect so a page can be drawn shifted and simply run off the top and
 * bottom of its pane. The console KDOS actually ships is 25 rows tall
 * (1280x800 with the 16x32 font); without this every long page would have to
 * be hand-tuned to fit, and the one that did not fit would silently lose its
 * last question. */
static KRect clipr;

/* How far down the page ASKED to draw, clipping ignored. The caller resets it
 * to the top of the pane, draws, and reads it back to learn the page's real
 * height — which is the scroll range. */
static int extent;

void ktui_extent_reset(int y)
{
	extent = y;
}

int ktui_extent(void)
{
	return extent;
}

/* ──────────────────────────────────────────────────────────────────────── */

const char *ktui_utf8_next(const char *s, uint32_t *cp)
{
	unsigned char c = (unsigned char)*s;
	if (c < 0x80) {
		*cp = c;
		return s + 1;
	}
	int extra;
	uint32_t v;
	if ((c & 0xe0) == 0xc0) {
		v = c & 0x1f;
		extra = 1;
	} else if ((c & 0xf0) == 0xe0) {
		v = c & 0x0f;
		extra = 2;
	} else if ((c & 0xf8) == 0xf0) {
		v = c & 0x07;
		extra = 3;
	} else {
		*cp = '?';
		return s + 1;
	}
	for (int i = 0; i < extra; i++) {
		if ((s[1 + i] & 0xc0) != 0x80) {
			*cp = '?';
			return s + 1;
		}
		v = (v << 6) | (uint32_t)(s[1 + i] & 0x3f);
	}
	*cp = v;
	return s + 1 + extra;
}

int ktui_utf8_width(const char *s)
{
	int n = 0;
	uint32_t cp;
	while (*s) {
		s = ktui_utf8_next(s, &cp);
		n++;
	}
	return n;
}

static int utf8_encode(uint32_t cp, char *out)
{
	if (cp < 0x80) {
		out[0] = (char)cp;
		return 1;
	}
	if (cp < 0x800) {
		out[0] = (char)(0xc0 | (cp >> 6));
		out[1] = (char)(0x80 | (cp & 0x3f));
		return 2;
	}
	if (cp < 0x10000) {
		out[0] = (char)(0xe0 | (cp >> 12));
		out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
		out[2] = (char)(0x80 | (cp & 0x3f));
		return 3;
	}
	out[0] = (char)(0xf0 | (cp >> 18));
	out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
	out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
	out[3] = (char)(0x80 | (cp & 0x3f));
	return 4;
}

/* ──────────────────────────────────────────────────────────────────────── */

int ktui_draw_init(void)
{
	const char **tbl = (ktui_caps & KT_CAP_UTF8) ? glyph_utf8 : glyph_ascii;
	for (int i = 0; i < KT_G_N; i++) {
		ktui_glyph[i] = tbl[i] ? tbl[i] : "?";
		uint32_t cp;
		ktui_utf8_next(ktui_glyph[i], &cp);
		glyph_cp[i] = cp;
	}
	ktui_ramp_init();
	ktui_draw_resize();
	return 0;
}

/* No ktui_term_init, so no tty, no ioctl and no signal handler: the size is
 * whatever the caller asked for. Everything downstream reads ktui_w/ktui_h, so
 * a screen drawn through here is the same code path a real terminal takes.
 *
 * `offscreen` is a one-way latch: nothing in this file ever clears it, so once
 * a process has called this there is no going back to a real terminal in the
 * same process — ktui_draw_flush() checks it forever after. */
int ktui_offscreen_init(int w, int h)
{
	if (w < 1 || h < 1)
		return 1;
	offscreen = 1;
	ktui_w = w;
	ktui_h = h;
	return ktui_draw_init();
}

/* The buffer as plain text — no escapes, no colour. A cell the frame never
 * touched holds 0 rather than a space, so it is spelled out here; otherwise a
 * dump has holes in it exactly where a screen was left blank. */
void ktui_draw_dump(void)
{
	char out[8];
	for (int y = 0; y < bh; y++) {
		for (int x = 0; x < bw; x++) {
			uint32_t ch = back[y * bw + x].ch;
			int n = utf8_encode(ch ? ch : ' ', out);
			fwrite(out, 1, (size_t)n, stdout);
		}
		fputc('\n', stdout);
	}
}

void ktui_draw_resize(void)
{
	if (bw == ktui_w && bh == ktui_h && front)
		return;
	free(front);
	free(back);
	bw = ktui_w;
	bh = ktui_h;
	clipr = krect(0, 0, bw, bh);
	front = kb_calloc((size_t)bw * bh, sizeof(KtuiCell));
	back = kb_calloc((size_t)bw * bh, sizeof(KtuiCell));
	force_full = 1;
}

void ktui_draw_invalidate(void)
{
	force_full = 1;
}

void ktui_draw_clear(void)
{
	for (int i = 0; i < bw * bh; i++) {
		back[i].ch = ' ';
		back[i].fg = KT_TEXT;
		back[i].bg = KT_BG;
		back[i].attr = 0;
	}
	ptr_x = ptr_y = -1;
}

void ktui_draw_clip(KRect r)
{
	clipr = r;
}

void ktui_draw_clip_none(void)
{
	clipr = krect(0, 0, bw, bh);
}

void ktui_draw_cell(int x, int y, uint32_t ch, int fg, int bg, int attr)
{
	/* Tracked BEFORE clipping: this is how the caller learns how tall the
	 * page wanted to be, which is what the scroll range is computed from. */
	if (y > extent)
		extent = y;
	if (!krect_hit(clipr, x, y))
		return;
	if (x < 0 || y < 0 || x >= bw || y >= bh)
		return;
	KtuiCell *c = &back[y * bw + x];
	c->ch = ch;
	c->fg = (uint8_t)fg;
	c->bg = (uint8_t)bg;
	c->attr = (uint8_t)attr;
}

void ktui_draw_fill(KRect r, int bg)
{
	for (int y = r.y; y < r.y + r.h; y++)
		for (int x = r.x; x < r.x + r.w; x++)
			ktui_draw_cell(x, y, ' ', KT_TEXT, bg, 0);
}

int ktui_draw_text(int x, int y, int maxw, const char *s, int fg, int bg, int attr)
{
	int n = 0;
	uint32_t cp;
	while (*s && n < maxw) {
		s = ktui_utf8_next(s, &cp);
		if (!(ktui_caps & KT_CAP_UTF8) && cp > 0x7f)
			cp = '?';
		ktui_draw_cell(x + n, y, cp, fg, bg, attr);
		n++;
	}
	return n;
}

int ktui_draw_textf(int x, int y, int maxw, int fg, int bg, int attr,
	       const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return ktui_draw_text(x, y, maxw, buf, fg, bg, attr);
}

int ktui_draw_text_right(int x, int y, int w, const char *s, int fg, int bg,
			 int attr)
{
	int tw = ktui_utf8_width(s);
	if (tw >= w)
		return ktui_draw_text(x, y, w, s, fg, bg, attr);
	return ktui_draw_text(x + w - tw, y, tw, s, fg, bg, attr);
}

void ktui_draw_hline(int x, int y, int w, int g, int fg, int bg)
{
	for (int i = 0; i < w; i++)
		ktui_draw_cell(x + i, y, glyph_cp[g], fg, bg, 0);
}

void ktui_draw_vline(int x, int y, int h, int g, int fg, int bg)
{
	for (int i = 0; i < h; i++)
		ktui_draw_cell(x, y + i, glyph_cp[g], fg, bg, 0);
}

void ktui_draw_box(KRect r, const char *title, int fg, int bg, int dbl)
{
	if (r.w < 2 || r.h < 2)
		return;
	int hl = dbl ? KT_G_DHL : KT_G_HL, vl = dbl ? KT_G_DVL : KT_G_VL;
	int tl = dbl ? KT_G_DTL : KT_G_TL, tr = dbl ? KT_G_DTR : KT_G_TR;
	int bl = dbl ? KT_G_DBL : KT_G_BL, br = dbl ? KT_G_DBR : KT_G_BR;

	ktui_draw_cell(r.x, r.y, glyph_cp[tl], fg, bg, 0);
	ktui_draw_cell(r.x + r.w - 1, r.y, glyph_cp[tr], fg, bg, 0);
	ktui_draw_cell(r.x, r.y + r.h - 1, glyph_cp[bl], fg, bg, 0);
	ktui_draw_cell(r.x + r.w - 1, r.y + r.h - 1, glyph_cp[br], fg, bg, 0);
	ktui_draw_hline(r.x + 1, r.y, r.w - 2, hl, fg, bg);
	ktui_draw_hline(r.x + 1, r.y + r.h - 1, r.w - 2, hl, fg, bg);
	ktui_draw_vline(r.x, r.y + 1, r.h - 2, vl, fg, bg);
	ktui_draw_vline(r.x + r.w - 1, r.y + 1, r.h - 2, vl, fg, bg);

	if (title && *title && r.w > 6) {
		int tw = ktui_utf8_width(title);
		if (tw > r.w - 6)
			tw = r.w - 6;
		ktui_draw_cell(r.x + 2, r.y, ' ', fg, bg, 0);
		ktui_draw_text(r.x + 3, r.y, tw, title, KT_ACCENT, bg, 0);
		ktui_draw_cell(r.x + 3 + tw, r.y, ' ', fg, bg, 0);
	}
}

/* A one-cell offset drop shadow. Cheap depth cue that survives eight colours:
 * the shadow is not a tint, it is the backdrop colour re-asserted. */
void ktui_draw_shadow(KRect r)
{
	for (int y = r.y + 1; y < r.y + r.h + 1 && y < bh; y++) {
		int x = r.x + r.w;
		if (x < bw) {
			KtuiCell *c = &back[y * bw + x];
			c->ch = ' ';
			c->bg = KT_BG;
			c->fg = KT_DIM;
			c->attr = 0;
		}
	}
	for (int x = r.x + 1; x < r.x + r.w + 1 && x < bw; x++) {
		int y = r.y + r.h;
		if (y < bh) {
			KtuiCell *c = &back[y * bw + x];
			c->ch = ' ';
			c->bg = KT_BG;
			c->fg = KT_DIM;
			c->attr = 0;
		}
	}
}

void ktui_draw_cursor(int x, int y)
{
	ptr_x = x;
	ptr_y = y;
}

void ktui_draw_hide_cursor(void)
{
	ptr_x = ptr_y = -1;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* On a VT the palette we installed makes slot == ANSI index, so the mapping
 * is the identity. Anywhere else the eight slots are approximated by the
 * terminal's own 16 — the layout survives, the exact hue does not. */
static const int ansi_fg[KT_NCOLOR] = { 30, 31, 92, 33, 90, 32, 30, 37 };
static const int ansi_bg[KT_NCOLOR] = { 40, 41, 42, 43, 100, 42, 40, 47 };

static int rgb_to_256(KRgb c)
{
	if (c.r == c.g && c.g == c.b) {
		if (c.r < 8)
			return 16;
		if (c.r > 248)
			return 231;
		return 232 + (c.r - 8) / 10;
	}
	int r = c.r * 5 / 255, g = c.g * 5 / 255, b = c.b * 5 / 255;
	return 16 + 36 * r + 6 * g + b;
}

static void emit_sgr(int fg, int bg, int attr)
{
	char buf[128];
	int n = 0;
	n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\033[0");

	if (attr & KT_A_REVERSE)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";7");
	if (attr & KT_A_UNDERLINE)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";4");
	/* Bold on a VT sets the intensity bit, which a 512-glyph font has
	 * repurposed as the 9th glyph bit — it changes the FONT PAGE, not the
	 * weight, and the text turns into line-noise. Never emit it there. */
	if ((attr & KT_A_BOLD) && !(ktui_caps & KT_CAP_LINUXVT))
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";1");

	if (ktui_caps & KT_CAP_TRUECOLOR) {
		KRgb f = ktui_theme->slot[fg], b = ktui_theme->slot[bg];
		n += snprintf(buf + n, sizeof(buf) - (size_t)n,
			      ";38;2;%u;%u;%u;48;2;%u;%u;%u",
			      f.r, f.g, f.b, b.r, b.g, b.b);
	} else if (ktui_caps & KT_CAP_256) {
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";38;5;%d;48;5;%d",
			      rgb_to_256(ktui_theme->slot[fg]),
			      rgb_to_256(ktui_theme->slot[bg]));
	} else if (ktui_caps & KT_CAP_LINUXVT) {
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";%d;%d",
			      30 + fg, 40 + bg);
	} else {
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";%d;%d",
			      ansi_fg[fg], ansi_bg[bg]);
	}

	n += snprintf(buf + n, sizeof(buf) - (size_t)n, "m");
	ktui_term_write(buf, (size_t)n);
}

void ktui_draw_flush(void)
{
	/* Offscreen there is nothing to present to, and stdout is where the
	 * dump goes — a flush would write escape sequences into it. It also has
	 * to leave `back` alone, because the dump reads it. Not hypothetical:
	 * ktui_toosmall() presents itself (kinstall and kdos-appbox return
	 * straight after calling it and never flush), so a preview at a size
	 * below the layout minimum came out as raw SGR. */
	if (offscreen)
		return;
	if (ptr_x >= 0 && ptr_x < bw && ptr_y >= 0 && ptr_y < bh)
		back[ptr_y * bw + ptr_x].attr ^= KT_A_REVERSE;

	int cur_fg = -1, cur_bg = -1, cur_attr = -1;
	int cx = -1, cy = -1;
	char utf[8];

	for (int y = 0; y < bh; y++) {
		for (int x = 0; x < bw; x++) {
			int i = y * bw + x;
			KtuiCell *b = &back[i], *f = &front[i];
			if (!force_full && b->ch == f->ch && b->fg == f->fg &&
			    b->bg == f->bg && b->attr == f->attr)
				continue;

			if (cy != y || cx != x) {
				ktui_term_printf("\033[%d;%dH", y + 1, x + 1);
				cx = x;
				cy = y;
			}
			if (b->fg != cur_fg || b->bg != cur_bg ||
			    b->attr != cur_attr) {
				emit_sgr(b->fg, b->bg, b->attr);
				cur_fg = b->fg;
				cur_bg = b->bg;
				cur_attr = b->attr;
			}
			int n = utf8_encode(b->ch ? b->ch : ' ', utf);
			ktui_term_write(utf, (size_t)n);
			cx++;
			*f = *b;
		}
	}

	ktui_term_write("\033[0m", 4);
	force_full = 0;
	ktui_term_flush();
}
