/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KDOS Installer — cell buffer and primitives
 * ---------------------------------
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kinstall.h"

/* Every glyph here was checked against uni/kdos.uni — the 512-glyph charset
 * ter-kdos32n is generated from. A character that is not in that file renders
 * as a blank on the TTY, which is why there is no ▓, no half block, and no
 * ← or → (the console font has ↑ and ↓ but not the horizontal pair; ◀ ▶ are
 * the ones that exist). Do not add a glyph to this table without grepping. */
static const char *glyph_utf8[G_N] = {
	[G_HL] = "─", [G_VL] = "│",
	[G_TL] = "┌", [G_TR] = "┐",
	[G_BL] = "└", [G_BR] = "┘",
	[G_TEE_L] = "├", [G_TEE_R] = "┤",
	[G_TEE_T] = "┬", [G_TEE_B] = "┴",
	[G_CROSS] = "┼",
	[G_DHL] = "═", [G_DVL] = "║",
	[G_DTL] = "╔", [G_DTR] = "╗",
	[G_DBL] = "╚", [G_DBR] = "╝",
	[G_FULL] = "█", [G_SHADE] = "░",
	[G_DOT] = "·", [G_BULLET] = "•", [G_SQUARE] = "■",
	[G_UP] = "↑", [G_DOWN] = "↓",
	[G_LEFT] = "◀", [G_RIGHT] = "▶",
	[G_ELLIPSIS] = "…", [G_DEG] = "°",
};

static const char *glyph_ascii[G_N] = {
	[G_HL] = "-", [G_VL] = "|",
	[G_TL] = "+", [G_TR] = "+", [G_BL] = "+", [G_BR] = "+",
	[G_TEE_L] = "+", [G_TEE_R] = "+", [G_TEE_T] = "+", [G_TEE_B] = "+",
	[G_CROSS] = "+",
	[G_DHL] = "=", [G_DVL] = "|",
	[G_DTL] = "+", [G_DTR] = "+", [G_DBL] = "+", [G_DBR] = "+",
	[G_FULL] = "#", [G_SHADE] = ".",
	[G_DOT] = ".", [G_BULLET] = "*", [G_SQUARE] = "#",
	[G_UP] = "^", [G_DOWN] = "v", [G_LEFT] = "<", [G_RIGHT] = ">",
	[G_ELLIPSIS] = "...", [G_DEG] = "o",
};

const char *ki_glyph[G_N];
static uint32_t glyph_cp[G_N];

static Cell *front, *back;
static int bw, bh;
static int force_full;
static int ptr_x = -1, ptr_y = -1;

/* A clip rect so a page can be drawn shifted and simply run off the top and
 * bottom of its pane. The console KDOS actually ships is 25 rows tall
 * (1280x800 with the 16x32 font); without this every long page would have to
 * be hand-tuned to fit, and the one that did not fit would silently lose its
 * last question. */
static Rect clipr;
int draw_maxy;

/* ──────────────────────────────────────────────────────────────────────── */

const char *utf8_next(const char *s, uint32_t *cp)
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

int utf8_width(const char *s)
{
	int n = 0;
	uint32_t cp;
	while (*s) {
		s = utf8_next(s, &cp);
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

int draw_init(void)
{
	const char **tbl = (term_caps & CAP_UTF8) ? glyph_utf8 : glyph_ascii;
	for (int i = 0; i < G_N; i++) {
		ki_glyph[i] = tbl[i] ? tbl[i] : "?";
		uint32_t cp;
		utf8_next(ki_glyph[i], &cp);
		glyph_cp[i] = cp;
	}
	draw_resize();
	return 0;
}

void draw_resize(void)
{
	if (bw == term_w && bh == term_h && front)
		return;
	free(front);
	free(back);
	bw = term_w;
	bh = term_h;
	clipr = rect(0, 0, bw, bh);
	front = xcalloc((size_t)bw * bh, sizeof(Cell));
	back = xcalloc((size_t)bw * bh, sizeof(Cell));
	force_full = 1;
}

void draw_invalidate(void)
{
	force_full = 1;
}

void draw_clear(void)
{
	for (int i = 0; i < bw * bh; i++) {
		back[i].ch = ' ';
		back[i].fg = CL_TEXT;
		back[i].bg = CL_BG;
		back[i].attr = 0;
	}
	ptr_x = ptr_y = -1;
}

void draw_clip(Rect r)
{
	clipr = r;
}

void draw_clip_none(void)
{
	clipr = rect(0, 0, bw, bh);
}

void draw_cell(int x, int y, uint32_t ch, int fg, int bg, int attr)
{
	/* Tracked BEFORE clipping: this is how the caller learns how tall the
	 * page wanted to be, which is what the scroll range is computed from. */
	if (y > draw_maxy)
		draw_maxy = y;
	if (!rect_hit(clipr, x, y))
		return;
	if (x < 0 || y < 0 || x >= bw || y >= bh)
		return;
	Cell *c = &back[y * bw + x];
	c->ch = ch;
	c->fg = (uint8_t)fg;
	c->bg = (uint8_t)bg;
	c->attr = (uint8_t)attr;
}

void draw_fill(Rect r, int bg)
{
	for (int y = r.y; y < r.y + r.h; y++)
		for (int x = r.x; x < r.x + r.w; x++)
			draw_cell(x, y, ' ', CL_TEXT, bg, 0);
}

int draw_text(int x, int y, int maxw, const char *s, int fg, int bg, int attr)
{
	int n = 0;
	uint32_t cp;
	while (*s && n < maxw) {
		s = utf8_next(s, &cp);
		if (!(term_caps & CAP_UTF8) && cp > 0x7f)
			cp = '?';
		draw_cell(x + n, y, cp, fg, bg, attr);
		n++;
	}
	return n;
}

int draw_textf(int x, int y, int maxw, int fg, int bg, int attr,
	       const char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return draw_text(x, y, maxw, buf, fg, bg, attr);
}

void draw_hline(int x, int y, int w, int g, int fg, int bg)
{
	for (int i = 0; i < w; i++)
		draw_cell(x + i, y, glyph_cp[g], fg, bg, 0);
}

void draw_vline(int x, int y, int h, int g, int fg, int bg)
{
	for (int i = 0; i < h; i++)
		draw_cell(x, y + i, glyph_cp[g], fg, bg, 0);
}

void draw_box(Rect r, const char *title, int fg, int bg, int dbl)
{
	if (r.w < 2 || r.h < 2)
		return;
	int hl = dbl ? G_DHL : G_HL, vl = dbl ? G_DVL : G_VL;
	int tl = dbl ? G_DTL : G_TL, tr = dbl ? G_DTR : G_TR;
	int bl = dbl ? G_DBL : G_BL, br = dbl ? G_DBR : G_BR;

	draw_cell(r.x, r.y, glyph_cp[tl], fg, bg, 0);
	draw_cell(r.x + r.w - 1, r.y, glyph_cp[tr], fg, bg, 0);
	draw_cell(r.x, r.y + r.h - 1, glyph_cp[bl], fg, bg, 0);
	draw_cell(r.x + r.w - 1, r.y + r.h - 1, glyph_cp[br], fg, bg, 0);
	draw_hline(r.x + 1, r.y, r.w - 2, hl, fg, bg);
	draw_hline(r.x + 1, r.y + r.h - 1, r.w - 2, hl, fg, bg);
	draw_vline(r.x, r.y + 1, r.h - 2, vl, fg, bg);
	draw_vline(r.x + r.w - 1, r.y + 1, r.h - 2, vl, fg, bg);

	if (title && *title && r.w > 6) {
		int tw = utf8_width(title);
		if (tw > r.w - 6)
			tw = r.w - 6;
		draw_cell(r.x + 2, r.y, ' ', fg, bg, 0);
		draw_text(r.x + 3, r.y, tw, title, CL_ACCENT, bg, 0);
		draw_cell(r.x + 3 + tw, r.y, ' ', fg, bg, 0);
	}
}

/* A one-cell offset drop shadow. Cheap depth cue that survives eight colours:
 * the shadow is not a tint, it is the backdrop colour re-asserted. */
void draw_shadow(Rect r)
{
	for (int y = r.y + 1; y < r.y + r.h + 1 && y < bh; y++) {
		int x = r.x + r.w;
		if (x < bw) {
			Cell *c = &back[y * bw + x];
			c->ch = ' ';
			c->bg = CL_BG;
			c->fg = CL_DIM;
			c->attr = 0;
		}
	}
	for (int x = r.x + 1; x < r.x + r.w + 1 && x < bw; x++) {
		int y = r.y + r.h;
		if (y < bh) {
			Cell *c = &back[y * bw + x];
			c->ch = ' ';
			c->bg = CL_BG;
			c->fg = CL_DIM;
			c->attr = 0;
		}
	}
}

void draw_cursor(int x, int y)
{
	ptr_x = x;
	ptr_y = y;
}

void draw_hide_cursor(void)
{
	ptr_x = ptr_y = -1;
}

/* ──────────────────────────────────────────────────────────────────────── */

/* On a VT the palette we installed makes slot == ANSI index, so the mapping
 * is the identity. Anywhere else the eight slots are approximated by the
 * terminal's own 16 — the layout survives, the exact hue does not. */
static const int ansi_fg[CL_N] = { 30, 31, 92, 33, 90, 32, 30, 37 };
static const int ansi_bg[CL_N] = { 40, 41, 42, 43, 100, 42, 40, 47 };

static int rgb_to_256(Rgb c)
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

	if (attr & A_REVERSE)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";7");
	if (attr & A_UNDERLINE)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";4");
	/* Bold on a VT sets the intensity bit, which a 512-glyph font has
	 * repurposed as the 9th glyph bit — it changes the FONT PAGE, not the
	 * weight, and the text turns into line-noise. Never emit it there. */
	if ((attr & A_BOLD) && !(term_caps & CAP_LINUXVT))
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";1");

	if (term_caps & CAP_TRUECOLOR) {
		Rgb f = ki_theme->slot[fg], b = ki_theme->slot[bg];
		n += snprintf(buf + n, sizeof(buf) - (size_t)n,
			      ";38;2;%u;%u;%u;48;2;%u;%u;%u",
			      f.r, f.g, f.b, b.r, b.g, b.b);
	} else if (term_caps & CAP_256) {
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";38;5;%d;48;5;%d",
			      rgb_to_256(ki_theme->slot[fg]),
			      rgb_to_256(ki_theme->slot[bg]));
	} else if (term_caps & CAP_LINUXVT) {
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";%d;%d",
			      30 + fg, 40 + bg);
	} else {
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";%d;%d",
			      ansi_fg[fg], ansi_bg[bg]);
	}

	n += snprintf(buf + n, sizeof(buf) - (size_t)n, "m");
	term_write(buf, (size_t)n);
}

void draw_flush(void)
{
	if (ptr_x >= 0 && ptr_x < bw && ptr_y >= 0 && ptr_y < bh)
		back[ptr_y * bw + ptr_x].attr ^= A_REVERSE;

	int cur_fg = -1, cur_bg = -1, cur_attr = -1;
	int cx = -1, cy = -1;
	char utf[8];

	for (int y = 0; y < bh; y++) {
		for (int x = 0; x < bw; x++) {
			int i = y * bw + x;
			Cell *b = &back[i], *f = &front[i];
			if (!force_full && b->ch == f->ch && b->fg == f->fg &&
			    b->bg == f->bg && b->attr == f->attr)
				continue;

			if (cy != y || cx != x) {
				term_printf("\033[%d;%dH", y + 1, x + 1);
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
			term_write(utf, (size_t)n);
			cx++;
			*f = *b;
		}
	}

	term_write("\033[0m", 4);
	force_full = 0;
	term_flush();
}
