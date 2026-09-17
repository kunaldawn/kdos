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

/*
 * THE FRAME A BACKEND LAST DIFFED AGAINST — not what is on the screen. A
 * backend need not maintain it: `kdos-con`'s does not, because a session with
 * several views holds one previous frame per view, and its `front` stays the
 * zeroed allocation for the life of the process. The one caller reads it
 * BESIDE ktui_draw_cells(), which is the sprite table asking whether a slot
 * is still referenced.
 *
 * NOT a general accessor. Nothing draws through this — every drawing path
 * goes through ktui_draw_cell, so the clip, the wide-glyph rules and the
 * damage diff all hold.
 *
 * The buffer's OWN size, not ktui_w by ktui_h. A backend reports a new size
 * the moment it is resized and the buffer is only reallocated when the
 * consumer calls ktui_draw_resize(), so between those two points the globals
 * describe a grid larger than the allocation — and a caller that walked
 * ktui_w * ktui_h cells would read past the end of it.
 */
const KtuiCell *ktui_cells(int *w, int *h)
{
	if (w)
		*w = front ? bw : 0;
	if (h)
		*h = front ? bh : 0;
	return front;
}

/*
 * THE FRAME BEING COMPOSED — what `ktui_draw_cell` writes and what the next
 * flush will send. Read it to find out what is on the screen.
 *
 * NOT `ktui_cells()`, WHICH IS A DIFFERENT BUFFER AND OFTEN AN EMPTY ONE. That
 * one hands out `front`, the frame a backend last diffed against, and a
 * backend is free never to maintain it: `kdos-con`'s own ignores `prev`
 * entirely, because there may be several views and each diffs against its own.
 * A caller that read `ktui_cells()` there got a screenful of zeroes and could
 * not tell that from a screenful of spaces.
 */
const KtuiCell *ktui_draw_cells(int *w, int *h)
{
	if (w)
		*w = back ? bw : 0;
	if (h)
		*h = back ? bh : 0;
	return back;
}
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

/* ────────────────────────────────────────────────────────────────────────
 * Cell width
 *
 * The grid's own wcwidth: 0 for combining marks, 2 for the East-Asian wide
 * and fullwidth blocks, 1 for everything else. Not the libc's — musl's
 * wcwidth answers for the locale, and every consumer taking its own view of
 * "how many cells is this" is how a CJK title corrupted three layouts at
 * once. Ranges over-approximate on the emoji blocks deliberately: a glyph
 * clipped to one cell beats one bleeding into its neighbour.
 * ──────────────────────────────────────────────────────────────────────── */

typedef struct {
	uint32_t first, last;
} CpRange;

static const CpRange width_zero[] = {
	{ 0x0300, 0x036f }, { 0x0483, 0x0489 }, { 0x0591, 0x05bd },
	{ 0x05bf, 0x05bf }, { 0x05c1, 0x05c2 }, { 0x05c4, 0x05c5 },
	{ 0x05c7, 0x05c7 }, { 0x0610, 0x061a }, { 0x064b, 0x065f },
	{ 0x0670, 0x0670 }, { 0x06d6, 0x06dc }, { 0x06df, 0x06e4 },
	{ 0x06e7, 0x06e8 }, { 0x06ea, 0x06ed }, { 0x0711, 0x0711 },
	{ 0x0730, 0x074a }, { 0x07a6, 0x07b0 }, { 0x07eb, 0x07f3 },
	{ 0x0816, 0x0819 }, { 0x081b, 0x0823 }, { 0x0825, 0x0827 },
	{ 0x0829, 0x082d }, { 0x0859, 0x085b }, { 0x08d3, 0x0902 },
	{ 0x093a, 0x093a }, { 0x093c, 0x093c }, { 0x0941, 0x0948 },
	{ 0x094d, 0x094d }, { 0x0951, 0x0957 }, { 0x0962, 0x0963 },
	{ 0x09bc, 0x09bc }, { 0x09c1, 0x09c4 }, { 0x09cd, 0x09cd },
	{ 0x09e2, 0x09e3 }, { 0x0a01, 0x0a02 }, { 0x0a3c, 0x0a3c },
	{ 0x0a41, 0x0a42 }, { 0x0a47, 0x0a48 }, { 0x0a4b, 0x0a4d },
	{ 0x0a51, 0x0a51 }, { 0x0a70, 0x0a71 }, { 0x0a75, 0x0a75 },
	{ 0x0a81, 0x0a82 }, { 0x0abc, 0x0abc }, { 0x0ac1, 0x0ac5 },
	{ 0x0ac7, 0x0ac8 }, { 0x0acd, 0x0acd }, { 0x0ae2, 0x0ae3 },
	{ 0x0b01, 0x0b01 }, { 0x0b3c, 0x0b3c }, { 0x0b3f, 0x0b3f },
	{ 0x0b41, 0x0b44 }, { 0x0b4d, 0x0b4d }, { 0x0b56, 0x0b56 },
	{ 0x0b62, 0x0b63 }, { 0x0b82, 0x0b82 }, { 0x0bc0, 0x0bc0 },
	{ 0x0bcd, 0x0bcd }, { 0x0c00, 0x0c00 }, { 0x0c3e, 0x0c40 },
	{ 0x0c46, 0x0c48 }, { 0x0c4a, 0x0c4d }, { 0x0c55, 0x0c56 },
	{ 0x0c62, 0x0c63 }, { 0x0c81, 0x0c81 }, { 0x0cbc, 0x0cbc },
	{ 0x0cbf, 0x0cbf }, { 0x0cc6, 0x0cc6 }, { 0x0ccc, 0x0ccd },
	{ 0x0ce2, 0x0ce3 }, { 0x0d01, 0x0d01 }, { 0x0d41, 0x0d44 },
	{ 0x0d4d, 0x0d4d }, { 0x0d62, 0x0d63 }, { 0x0dca, 0x0dca },
	{ 0x0dd2, 0x0dd4 }, { 0x0dd6, 0x0dd6 }, { 0x0e31, 0x0e31 },
	{ 0x0e34, 0x0e3a }, { 0x0e47, 0x0e4e }, { 0x0eb1, 0x0eb1 },
	{ 0x0eb4, 0x0ebc }, { 0x0ec8, 0x0ecd }, { 0x0f18, 0x0f19 },
	{ 0x0f35, 0x0f35 }, { 0x0f37, 0x0f37 }, { 0x0f39, 0x0f39 },
	{ 0x0f71, 0x0f7e }, { 0x0f80, 0x0f84 }, { 0x0f86, 0x0f87 },
	{ 0x0f8d, 0x0f97 }, { 0x0f99, 0x0fbc }, { 0x0fc6, 0x0fc6 },
	{ 0x102d, 0x1030 }, { 0x1032, 0x1037 }, { 0x1039, 0x103a },
	{ 0x103d, 0x103e }, { 0x1058, 0x1059 }, { 0x105e, 0x1060 },
	{ 0x1071, 0x1074 }, { 0x1082, 0x1082 }, { 0x1085, 0x1086 },
	{ 0x108d, 0x108d }, { 0x109d, 0x109d }, { 0x135d, 0x135f },
	{ 0x1712, 0x1714 }, { 0x1732, 0x1734 }, { 0x1752, 0x1753 },
	{ 0x1772, 0x1773 }, { 0x17b4, 0x17b5 }, { 0x17b7, 0x17bd },
	{ 0x17c6, 0x17c6 }, { 0x17c9, 0x17d3 }, { 0x17dd, 0x17dd },
	{ 0x180b, 0x180e }, { 0x18a9, 0x18a9 }, { 0x1920, 0x1922 },
	{ 0x1927, 0x1928 }, { 0x1932, 0x1932 }, { 0x1939, 0x193b },
	{ 0x1a17, 0x1a18 }, { 0x1a56, 0x1a56 }, { 0x1a58, 0x1a5e },
	{ 0x1a60, 0x1a60 }, { 0x1a62, 0x1a62 }, { 0x1a65, 0x1a6c },
	{ 0x1a73, 0x1a7c }, { 0x1a7f, 0x1a7f }, { 0x1ab0, 0x1ace },
	{ 0x1b00, 0x1b03 }, { 0x1b34, 0x1b34 }, { 0x1b36, 0x1b3a },
	{ 0x1b3c, 0x1b3c }, { 0x1b42, 0x1b42 }, { 0x1b6b, 0x1b73 },
	{ 0x1b80, 0x1b81 }, { 0x1ba2, 0x1ba5 }, { 0x1ba8, 0x1ba9 },
	{ 0x1bab, 0x1bad }, { 0x1be6, 0x1be6 }, { 0x1be8, 0x1be9 },
	{ 0x1bed, 0x1bed }, { 0x1bef, 0x1bf1 }, { 0x1c2c, 0x1c33 },
	{ 0x1c36, 0x1c37 }, { 0x1cd0, 0x1cd2 }, { 0x1cd4, 0x1ce0 },
	{ 0x1ce2, 0x1ce8 }, { 0x1ced, 0x1ced }, { 0x1cf4, 0x1cf4 },
	{ 0x1cf8, 0x1cf9 }, { 0x1dc0, 0x1dff }, { 0x200b, 0x200f },
	{ 0x202a, 0x202e }, { 0x2060, 0x2064 }, { 0x20d0, 0x20ff },
	{ 0x2cef, 0x2cf1 }, { 0x2d7f, 0x2d7f }, { 0x2de0, 0x2dff },
	{ 0x302a, 0x302d }, { 0x3099, 0x309a }, { 0xa66f, 0xa672 },
	{ 0xa674, 0xa67d }, { 0xa69e, 0xa69f }, { 0xa6f0, 0xa6f1 },
	{ 0xa802, 0xa802 }, { 0xa806, 0xa806 }, { 0xa80b, 0xa80b },
	{ 0xa825, 0xa826 }, { 0xa8c4, 0xa8c5 }, { 0xa8e0, 0xa8f1 },
	{ 0xa926, 0xa92d }, { 0xa947, 0xa951 }, { 0xa980, 0xa982 },
	{ 0xa9b3, 0xa9b3 }, { 0xa9b6, 0xa9b9 }, { 0xa9bc, 0xa9bd },
	{ 0xaa29, 0xaa2e }, { 0xaa31, 0xaa32 }, { 0xaa35, 0xaa36 },
	{ 0xaa43, 0xaa43 }, { 0xaa4c, 0xaa4c }, { 0xaab0, 0xaab0 },
	{ 0xaab2, 0xaab4 }, { 0xaab7, 0xaab8 }, { 0xaabe, 0xaabf },
	{ 0xaac1, 0xaac1 }, { 0xaaec, 0xaaed }, { 0xaaf6, 0xaaf6 },
	{ 0xabe5, 0xabe5 }, { 0xabe8, 0xabe8 }, { 0xabed, 0xabed },
	{ 0xfb1e, 0xfb1e }, { 0xfe00, 0xfe0f }, { 0xfe20, 0xfe2f },
	{ 0xfeff, 0xfeff }, { 0x101fd, 0x101fd }, { 0x10a01, 0x10a0f },
	{ 0x11038, 0x11046 }, { 0x1d167, 0x1d169 }, { 0x1d17b, 0x1d182 },
	{ 0x1d185, 0x1d18b }, { 0x1d1aa, 0x1d1ad }, { 0xe0100, 0xe01ef },
};

static const CpRange width_two[] = {
	{ 0x1100, 0x115f }, { 0x231a, 0x231b }, { 0x2329, 0x232a },
	{ 0x23e9, 0x23ec }, { 0x23f0, 0x23f0 }, { 0x23f3, 0x23f3 },
	{ 0x25fd, 0x25fe }, { 0x2614, 0x2615 }, { 0x2648, 0x2653 },
	{ 0x267f, 0x267f }, { 0x2693, 0x2693 }, { 0x26a1, 0x26a1 },
	{ 0x26aa, 0x26ab }, { 0x26bd, 0x26be }, { 0x26c4, 0x26c5 },
	{ 0x26ce, 0x26ce }, { 0x26d4, 0x26d4 }, { 0x26ea, 0x26ea },
	{ 0x26f2, 0x26f3 }, { 0x26f5, 0x26f5 }, { 0x26fa, 0x26fa },
	{ 0x26fd, 0x26fd }, { 0x2705, 0x2705 }, { 0x270a, 0x270b },
	{ 0x2728, 0x2728 }, { 0x274c, 0x274c }, { 0x274e, 0x274e },
	{ 0x2753, 0x2755 }, { 0x2757, 0x2757 }, { 0x2795, 0x2797 },
	{ 0x27b0, 0x27b0 }, { 0x27bf, 0x27bf }, { 0x2b1b, 0x2b1c },
	{ 0x2b50, 0x2b50 }, { 0x2b55, 0x2b55 }, { 0x2e80, 0x303e },
	{ 0x3041, 0x33ff }, { 0x3400, 0x4dbf }, { 0x4e00, 0x9fff },
	{ 0xa000, 0xa4cf }, { 0xa960, 0xa97f }, { 0xac00, 0xd7a3 },
	{ 0xf900, 0xfaff }, { 0xfe10, 0xfe19 }, { 0xfe30, 0xfe52 },
	{ 0xfe54, 0xfe66 }, { 0xfe68, 0xfe6b }, { 0xff00, 0xff60 },
	{ 0xffe0, 0xffe6 }, { 0x16fe0, 0x16fe4 }, { 0x17000, 0x187f7 },
	{ 0x18800, 0x18cd5 }, { 0x1b000, 0x1b2fb }, { 0x1f004, 0x1f004 },
	{ 0x1f0cf, 0x1f0cf }, { 0x1f18e, 0x1f18e }, { 0x1f191, 0x1f19a },
	{ 0x1f200, 0x1f265 }, { 0x1f300, 0x1f64f }, { 0x1f680, 0x1f6ff },
	{ 0x1f7e0, 0x1f7eb }, { 0x1f900, 0x1f9ff }, { 0x1fa00, 0x1faff },
	{ 0x20000, 0x2fffd }, { 0x30000, 0x3fffd },
};

static int cp_in(const CpRange *t, int n, uint32_t cp)
{
	int lo = 0, hi = n - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (cp < t[mid].first)
			hi = mid - 1;
		else if (cp > t[mid].last)
			lo = mid + 1;
		else
			return 1;
	}
	return 0;
}

int ktui_wcwidth(uint32_t cp)
{
	if (cp < 0x0300)
		return 1;
	if (cp_in(width_zero, (int)(sizeof(width_zero) / sizeof(*width_zero)), cp))
		return 0;
	if (cp_in(width_two, (int)(sizeof(width_two) / sizeof(*width_two)), cp))
		return 2;
	return 1;
}

int ktui_utf8_width(const char *s)
{
	int n = 0;
	uint32_t cp;
	while (*s) {
		s = ktui_utf8_next(s, &cp);
		n += ktui_wcwidth(cp);
	}
	return n;
}

int ktui_utf8_encode(uint32_t cp, char *out)
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

/* Defined with the tty backend below; the drawing entry points above it need
 * to ask it for the size. NULL means "nobody chose", which resolves to the tty
 * — so a program that never calls ktui_backend_set() behaves exactly as it did
 * before backends existed. */
static const KtuiBackend backend_tty;
static const KtuiBackend *backend;

static const KtuiBackend *cur_backend(void)
{
	return backend ? backend : &backend_tty;
}

/*
 * Take the size and the capabilities from whichever backend is installed.
 *
 * ktui_w/ktui_h/ktui_caps used to be set only by ktui_term_init(), which meant
 * every drawing path silently assumed a terminal had been opened first. A
 * Wayland surface has neither an ioctl nor a TERM to answer that question — it
 * learns its size from a configure event — so without this the cell buffer was
 * allocated 0x0 and the first fill ran off the end of it. Found by segfault,
 * not by reading.
 *
 * Offscreen is exempt: its size is the caller's declaration, not a
 * measurement, and re-reading it would overwrite the very thing being asked
 * for.
 */
static void backend_sync(void)
{
	if (offscreen)
		return;
	ktui_caps = cur_backend()->caps();
	cur_backend()->size(&ktui_w, &ktui_h);
}

int ktui_draw_init(void)
{
	/* Before the glyph table is chosen: the tier follows from KT_CAP_UTF8,
	 * so the caps have to be the backend's by now. */
	backend_sync();
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
			/* A continuation cell is the right half of the wide
			 * glyph before it, which already reads as two columns
			 * in a monospace viewer — emitting anything for it
			 * would shear every column after it. Unless it was
			 * ORPHANED: a narrow glyph drawn over the lead leaves
			 * a continuation nothing covers, and skipping that one
			 * shears the row the other way. */
			if (ch == KTUI_WIDE_CONT) {
				if (x > 0 &&
				    ktui_wcwidth(back[y * bw + x - 1].ch) == 2)
					continue;
				ch = ' ';
			}
			/* A sprite has no codepoint. What a text view shows is
			 * the fallback the caller registered, in the sprite's
			 * top-left cell only — four identical blocks for one
			 * 2x2 icon merge with the icon beside it, and a dump
			 * exists to be read. */
			ch = ktui_sprite_text_cell(ch);
			int n = ktui_utf8_encode(ch ? ch : ' ', out);
			fwrite(out, 1, (size_t)n, stdout);
		}
		fputc('\n', stdout);
	}
}

void ktui_draw_resize(void)
{
	backend_sync();
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

/*
 * A RECTANGLE OWES A REPAINT EVEN THOUGH ITS CELLS DID NOT CHANGE.
 *
 * The flush repaints what differs between the frame being drawn and the one on
 * screen, which is right for everything a cell describes by value and wrong
 * for the one thing it describes by reference: a sprite cell names a SLOT, so
 * an animation's next frame writes byte-identical cells and the diff finds
 * nothing. Spoiling the previous-frame copy is what says otherwise, and it is
 * the same mechanism the pointer's XOR already relies on.
 *
 * A RECTANGLE RATHER THAN THE SCREEN, because the alternative — a full repaint
 * per arriving picture — makes an embedded application cost the whole grid and
 * the whole framebuffer upload on every tile it sends, which is dozens of
 * times per frame. Nothing is a real cell here: 0xffffffff is above Unicode's
 * last codepoint and is not KTUI_SPRITE_BASE, so it can never equal one.
 *
 * AND THE BACKEND IS TOLD AS WELL, because a backend may diff against a copy
 * of its own rather than this one — libkkms keeps a previous frame per SCREEN,
 * where this is per session. Spoiling only this copy leaves such a backend
 * seeing no difference at all, which is an animation frozen on its first
 * frame. A backend with no `dirty` reads `prev` and needs nothing more.
 */
void ktui_draw_dirty(int x, int y, int w, int h)
{
	if (!front || w < 1 || h < 1)
		return;
	if (x < 0) {
		w += x;
		x = 0;
	}
	if (y < 0) {
		h += y;
		y = 0;
	}
	if (x + w > bw)
		w = bw - x;
	if (y + h > bh)
		h = bh - y;
	if (w < 1 || h < 1)
		return;
	for (int r = y; r < y + h; r++)
		for (int c = x; c < x + w; c++)
			front[(size_t)r * bw + c].ch = 0xffffffffu;
	if (cur_backend()->dirty)
		cur_backend()->dirty(x, y, w, h);
}

void ktui_draw_clear(void)
{
	/*
	 * ONE ROW BUILT AND THEN COPIED. Every field of every cell was
	 * assigned in a scalar loop, and this runs at the top of every frame
	 * of every surface — the first row is the only one that has to be
	 * written a field at a time, and the rest are a memcpy the compiler
	 * and the C library already vectorise.
	 *
	 * The literals go too: a cell is compared WHOLE by everything
	 * downstream, so a clear that left `fgc` holding the last frame's
	 * colour is a cell that differs from a blank one and is re-sent and
	 * repainted for ever.
	 */
	KtuiCell blank = { .ch = ' ', .fg = KT_TEXT, .bg = KT_BG,
			   .attr = 0, .fgc = 0, .bgc = 0, .ulc = 0 };

	if (bw > 0 && bh > 0) {
		for (int x = 0; x < bw; x++)
			back[x] = blank;
		for (int y = 1; y < bh; y++)
			memcpy(back + (size_t)y * bw, back,
			       (size_t)bw * sizeof(*back));
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
	/* The high bits are a colour run's and cannot be reached from here:
	 * a caller drawing in slots that set KT_A_FGRGB, KT_A_BGRGB or
	 * KT_A_ULCOLOR would name a colour it never supplied. */
	c->attr = (uint16_t)attr & 0xffu;
	c->fgc = c->bgc = c->ulc = 0;
}

void ktui_draw_put(int x, int y, const KtuiCell *cell)
{
	if (y > extent)
		extent = y;
	if (!krect_hit(clipr, x, y))
		return;
	if (x < 0 || y < 0 || x >= bw || y >= bh)
		return;
	back[y * bw + x] = *cell;
}

/*
 * A RECTANGLE OF BACKDROP, CLIPPED ONCE AND WRITTEN BY THE ROW.
 *
 * Every surface fills its whole pane at the top of every frame. Going cell by
 * cell through ktui_draw_cell costs a call, an extent update and two
 * rectangle tests apiece — tens of thousands of them on a console-sized grid,
 * every one reaching the same answer. The rect meets the clip and the buffer
 * once here; what is left is one row of struct copies and a memcpy for each
 * row below it.
 */
void ktui_draw_fill(KRect r, int bg)
{
	if (r.w <= 0 || r.h <= 0)
		return;
	/* Tracked BEFORE clipping, for the reason ktui_draw_cell gives. */
	if (r.y + r.h - 1 > extent)
		extent = r.y + r.h - 1;

	int x0 = r.x, y0 = r.y, x1 = r.x + r.w, y1 = r.y + r.h;

	if (x0 < clipr.x)
		x0 = clipr.x;
	if (y0 < clipr.y)
		y0 = clipr.y;
	if (x1 > clipr.x + clipr.w)
		x1 = clipr.x + clipr.w;
	if (y1 > clipr.y + clipr.h)
		y1 = clipr.y + clipr.h;
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > bw)
		x1 = bw;
	if (y1 > bh)
		y1 = bh;
	if (x0 >= x1 || y0 >= y1)
		return;

	/* The literals go too, as they do in ktui_draw_clear: a cell is
	 * compared WHOLE downstream, so a fill that left `fgc` holding the
	 * last frame's colour differs from a visually identical cell and is
	 * re-encoded and re-sent for ever. */
	const KtuiCell cell = { .ch = ' ', .fg = KT_TEXT, .bg = (uint8_t)bg,
				.attr = 0, .fgc = 0, .bgc = 0, .ulc = 0 };
	KtuiCell *row = back + (size_t)y0 * bw + x0;
	size_t n = (size_t)(x1 - x0);

	for (size_t i = 0; i < n; i++)
		row[i] = cell;
	for (int y = y0 + 1; y < y1; y++)
		memcpy(back + (size_t)y * bw + x0, row, n * sizeof(*back));
}

int ktui_draw_text(int x, int y, int maxw, const char *s, int fg, int bg, int attr)
{
	int n = 0;
	uint32_t cp;
	while (*s && n < maxw) {
		s = ktui_utf8_next(s, &cp);
		if (!(ktui_caps & KT_CAP_UTF8) && cp > 0x7f)
			cp = '?';
		int cw = ktui_wcwidth(cp);
		/* The grid cannot compose: a combining mark gets no cell of its
		 * own, which keeps the columns in line with ktui_utf8_width. */
		if (cw == 0)
			continue;
		if (cw == 2) {
			/* The VT font is 512 glyphs and every CJK codepoint
			 * renders single-width blank there — a wide pair would
			 * desync the tty diff's cursor arithmetic. */
			if (ktui_caps & KT_CAP_LINUXVT) {
				cp = '?';
				cw = 1;
			} else if (n + 2 > maxw) {
				/* No room for both halves at the row end. */
				ktui_draw_cell(x + n, y, ' ', fg, bg, attr);
				n++;
				break;
			}
		}
		ktui_draw_cell(x + n, y, cp, fg, bg, attr);
		if (cw == 2)
			ktui_draw_cell(x + n + 1, y, KTUI_WIDE_CONT, fg, bg, attr);
		n += cw;
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

/*
 * A one-cell offset drop shadow. Cheap depth cue that survives eight colours:
 * the shadow is not a tint, it is the backdrop colour re-asserted.
 *
 * It goes through ktui_draw_cell like every other primitive, so the clip
 * holds and the literal colour fields are cleared — a cell is compared WHOLE
 * downstream, and a shadow left holding the literals of whatever it covered
 * differs from an identical shadow elsewhere and is re-encoded and re-sent
 * every frame.
 *
 * The extent is put back afterwards: a shadow is decoration hanging one cell
 * outside the rect, and letting it grow the page's reported height would add
 * a phantom row to every scroll range measured around a shadowed box.
 */
void ktui_draw_shadow(KRect r)
{
	int keep = extent;

	for (int y = r.y + 1; y < r.y + r.h + 1; y++)
		ktui_draw_cell(r.x + r.w, y, ' ', KT_DIM, KT_BG, 0);
	for (int x = r.x + 1; x < r.x + r.w + 1; x++)
		ktui_draw_cell(x, r.y + r.h, ' ', KT_DIM, KT_BG, 0);
	extent = keep;
}

/*
 * XOR the reverse attribute over a rectangle of the frame being composed.
 *
 * A SELECTION IS NOT A REPAINT. The cells under it belong to whatever drew
 * them — a terminal's output, a surface, a picture's fallback — and a caller
 * that filled them with a colour of its own would hide the text a person is
 * selecting the extent of. Reversing is the one transform that leaves the
 * content and changes only how it reads.
 *
 * It works on `back`, the frame being composed, and must be called after
 * everything under it is drawn. `ktui_cells()` is the wrong source for this:
 * it hands out `front`, which is the frame that was last FLUSHED, so a caller
 * reading it mid-compose reverses the cells of the previous picture.
 */
void ktui_draw_reverse(KRect r)
{
	for (int y = r.y; y < r.y + r.h; y++) {
		if (y < 0 || y >= bh)
			continue;
		for (int x = r.x; x < r.x + r.w; x++) {
			if (x < 0 || x >= bw)
				continue;
			back[y * bw + x].attr ^= KT_A_REVERSE;
		}
	}
}

/*
 * ── TRANSLUCENCY ────────────────────────────────────────────────────
 *
 * A COMPOSED GRID HAS NOTHING UNDERNEATH IT. One cell is one place, so a
 * caller that wants to see through a window has to keep what was there before
 * it drew: ktui_draw_bg_take() copies the rectangle's background colours out,
 * the caller draws its window, and ktui_draw_blend() mixes the result back
 * towards them.
 *
 * THE LITERAL ONLY, NEVER THE SLOT. The blend is a colour the palette does not
 * hold, so it is written as the cell's own `bgc` and the slot is left as the
 * caller drew it: a display that declined the colour run — a --tty view, a
 * golden, a braille reader — shows an opaque window, which is the honest
 * answer where there is no colour to mix with.
 *
 * BACKGROUNDS AND NOT INK. A translucent glyph is a glyph nobody can read, so
 * the foreground stays exactly the colour it was drawn in.
 */

/*
 * The colour a composed cell's background is ACTUALLY PAINTED IN, and under
 * KT_A_REVERSE that is the foreground's: reverse is an exchange of what is
 * drawn and what is behind it, and the painter swaps the slot and its literal
 * together. A blend that read `bg` through a reversed cell would make the INK
 * translucent and leave the background opaque, which is the two defects this
 * pass exists to avoid, at once.
 */
static uint32_t bg_rgb_at(const KtuiCell *c)
{
	int rev = (c->attr & KT_A_REVERSE) != 0;
	KRgb k;

	if (c->attr & (rev ? KT_A_FGRGB : KT_A_BGRGB))
		return (rev ? c->fgc : c->bgc) & 0xffffffu;
	k = ktui_theme->slot[(rev ? c->fg : c->bg) & 7];
	return (uint32_t)k.r << 16 | (uint32_t)k.g << 8 | k.b;
}

void ktui_draw_bg_take(KRect r, uint32_t *out)
{
	for (int y = 0; y < r.h; y++) {
		for (int x = 0; x < r.w; x++) {
			int cx = r.x + x, cy = r.y + y;

			out[(size_t)y * r.w + x] =
				cx >= 0 && cy >= 0 && cx < bw && cy < bh
					? bg_rgb_at(&back[cy * bw + cx])
					: 0;
		}
	}
}

void ktui_draw_blend(KRect r, const uint32_t *under, int alpha)
{
	if (alpha >= 255)
		return;
	if (alpha < 0)
		alpha = 0;
	for (int y = 0; y < r.h; y++) {
		for (int x = 0; x < r.w; x++) {
			int cx = r.x + x, cy = r.y + y;
			KtuiCell *c;
			uint32_t o, u, mix = 0;

			if (cx < 0 || cy < 0 || cx >= bw || cy >= bh)
				continue;
			if (!krect_hit(clipr, cx, cy))
				continue;
			c = &back[cy * bw + cx];
			/*
			 * A PICTURE IS NOT A BACKGROUND. A sprite cell's
			 * pixels are somebody else's whole image, and the
			 * colour behind them is never shown — mixing it
			 * would cost the work and change nothing.
			 */
			if (KTUI_IS_SPRITE(c->ch))
				continue;
			o = bg_rgb_at(c);
			u = under[(size_t)y * r.w + x];
			for (int sh = 0; sh <= 16; sh += 8) {
				unsigned a = (o >> sh) & 0xffu;
				unsigned b = (u >> sh) & 0xffu;

				mix |= ((a * (unsigned)alpha +
					 b * (255u - (unsigned)alpha)) / 255u)
				       << sh;
			}
			if (c->attr & KT_A_REVERSE) {
				c->fgc = mix;
				c->attr |= KT_A_FGRGB;
			} else {
				c->bgc = mix;
				c->attr |= KT_A_BGRGB;
			}
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

/*
 * The nearest 6x6x6 cube level for one channel.
 *
 * The cube's levels are {0, 95, 135, 175, 215, 255} — not a linear ramp, so
 * the nearest level is a comparison against their midpoints, not a division.
 * Dividing by 255 quantises against {0, 51, 102, 153, 204, 255} instead and
 * pushes every colour that IS an exact cube colour, which is what the shipped
 * schemes are built from, a step off itself.
 */
static int cube_lvl(int v)
{
	return v < 48 ? 0 : v < 115 ? 1 : v < 155 ? 2 : v < 195 ? 3
	     : v < 235 ? 4 : 5;
}

static int rgb_to_256(KRgb c)
{
	if (c.r == c.g && c.g == c.b) {
		if (c.r < 8)
			return 16;
		/*
		 * The ramp's top step is index 255 = 238 and the cube's white
		 * is 231 = 255, so 247 upward is nearer white than any grey
		 * the ramp carries. The bound also has to be at most 248:
		 * 232 + (248 - 8) / 10 is 256, which is not a colour, and the
		 * terminal gets `38;5;256` and draws whatever it makes of it.
		 */
		if (c.r >= 247)
			return 231;
		return 232 + (c.r - 8) / 10;
	}
	int r = cube_lvl(c.r);
	int g = cube_lvl(c.g);
	int b = cube_lvl(c.b);

	return 16 + 36 * r + 6 * g + b;
}

/* A literal as the palette's own type, so both colour paths below take the
 * same argument whatever it came from. */
static KRgb rgb_krgb(uint32_t v)
{
	KRgb c = { (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };

	return c;
}

/*
 * The SGR for one cell.
 *
 * It takes the CELL rather than three numbers because a colour may be a slot
 * or a literal and the choice is per cell: the slot is what follows
 * `kdos theme` and the literal is what a program asked for exactly, and every
 * consumer that was never sent a literal still has the slot to draw.
 */
/*
 * THE SGR FOR A CELL WHOSE COLOURS ARE SLOTS, BUILT ONCE PER COMBINATION.
 *
 * Every style change rebuilds the sequence from up to nine snprintf calls, and
 * a frame of a coloured terminal changes style thousands of times. The slots
 * and the drawable attributes are a small space — eight by eight by the six
 * bits below the underline style — and what a combination produces depends
 * only on them and on the palette. A cell naming a literal colour is not
 * cached: those are unbounded and change every frame anyway.
 */
#define SGR_SLOTS (8 * 8 * 64)

static char sgr_cache[SGR_SLOTS][48];
static unsigned char sgr_len[SGR_SLOTS];
static KRgb sgr_slot[KT_NCOLOR];
static int sgr_caps = -1;

/*
 * DROP THE CACHED SEQUENCES WHEN THE PALETTE OR THE TERMINAL MOVED UNDER
 * THEM.
 *
 * The test is on the eight COLOURS, not on the address of `ktui_theme`: night
 * light projects the chosen scheme into a static buffer and rewrites that
 * buffer in place, so the pointer does not move when the scheme changes while
 * the toggle is on, and an address is therefore not a version of the palette.
 *
 * Called once per frame rather than once per style change. A palette cannot
 * move while a frame is being written — nothing on this path yields — so
 * every sequence a frame emits describes the same palette, and the per-cell
 * hot path pays nothing for the check.
 */
static void sgr_check(void)
{
	if (sgr_caps == ktui_caps &&
	    !memcmp(sgr_slot, ktui_theme->slot, sizeof(sgr_slot)))
		return;
	memcpy(sgr_slot, ktui_theme->slot, sizeof(sgr_slot));
	sgr_caps = ktui_caps;
	memset(sgr_len, 0, sizeof(sgr_len));
}

static void emit_sgr(const KtuiCell *cell)
{
	char buf[192];
	unsigned attr = cell->attr;

	/*
	 * A cell naming a literal colour is not cached — those are unbounded —
	 * and neither is a styled underline, whose style sits above the six
	 * bits the key is built from and would otherwise share a slot with the
	 * plain line.
	 */
	int cacheable = !(attr & (KT_A_FGRGB | KT_A_BGRGB | KT_A_ULCOLOR |
				  KT_A_ULSTYLE));
	unsigned slot = 0;

	if (cacheable) {
		slot = ((unsigned)(cell->fg & 7) << 9) |
		       ((unsigned)(cell->bg & 7) << 6) | (attr & 0x3fu);
		if (sgr_len[slot]) {
			ktui_term_write(sgr_cache[slot], sgr_len[slot]);
			return;
		}
	}
	int vt = (ktui_caps & KT_CAP_LINUXVT) != 0;
	/*
	 * A STYLED UNDERLINE AND ITS COLOUR GO ONLY WHERE 24-BIT COLOUR DOES.
	 * `4:3` is a sub-parameter, and a terminal old enough to want indexed
	 * colour is old enough to drop the colon and read the pair as SGR 43 —
	 * a green background where a program asked for a wavy line. There is
	 * no probe for it, so the capability we do have stands in for it.
	 */
	int rich = (ktui_caps & KT_CAP_TRUECOLOR) && !vt;
	int n = 0;

	n += snprintf(buf + n, sizeof(buf) - (size_t)n, "\033[0");

	if (attr & KT_A_REVERSE)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";7");
	/* A Linux VT has no underline attribute: it remaps SGR 4 onto a COLOUR
	 * the palette does not own, so an underlined row there comes out a
	 * shade nothing else on the screen uses. The guard is the same one
	 * bold keeps three lines down, and for the same class of reason. */
	if ((attr & KT_A_UNDERLINE) && !vt) {
		unsigned st = KT_UL_STYLE(attr);

		if (rich && st > KT_UL_SINGLE)
			n += snprintf(buf + n, sizeof(buf) - (size_t)n,
				      ";4:%u", st);
		else
			n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";4");
	}
	/* Bold on a VT sets the intensity bit, which a 512-glyph font has
	 * repurposed as the 9th glyph bit — it changes the FONT PAGE, not the
	 * weight, and the text turns into line-noise. Never emit it there. */
	if ((attr & KT_A_BOLD) && !vt)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";1");
	/* Italic, strikethrough and overline carry a terminal's own text
	 * through this desktop unchanged, and they share bold's guard for
	 * bold's reason: the VT has no style bits to set, only font pages and
	 * a colour it would take these for. */
	if ((attr & KT_A_ITALIC) && !vt)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";3");
	if ((attr & KT_A_STRIKE) && !vt)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";9");
	if ((attr & KT_A_OVERLINE) && !vt)
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";53");
	if (rich && (attr & KT_A_ULCOLOR)) {
		KRgb u = rgb_krgb(cell->ulc);

		n += snprintf(buf + n, sizeof(buf) - (size_t)n,
			      ";58:2::%u:%u:%u", u.r, u.g, u.b);
	}

	/*
	 * EVERY SLOT INDEX IS MASKED TO THREE BITS. `fg` and `bg` are a whole
	 * byte of the cell and nothing on the way in narrows them:
	 * ktui_draw_put() copies a caller's cell wholesale and libkcon's wire
	 * decoder takes the byte straight off the socket, so a program on the
	 * session bus can name slot 200. The palette and the ANSI tables below
	 * are eight entries, and the cache key masks as well — an unmasked
	 * lookup would both read past the array and file the stray colour
	 * under a real slot's key.
	 */
	if (ktui_caps & KT_CAP_TRUECOLOR) {
		KRgb f = (attr & KT_A_FGRGB) ? rgb_krgb(cell->fgc)
					     : ktui_theme->slot[cell->fg & 7];
		KRgb b = (attr & KT_A_BGRGB) ? rgb_krgb(cell->bgc)
					     : ktui_theme->slot[cell->bg & 7];

		n += snprintf(buf + n, sizeof(buf) - (size_t)n,
			      ";38;2;%u;%u;%u;48;2;%u;%u;%u",
			      f.r, f.g, f.b, b.r, b.g, b.b);
	} else if (ktui_caps & KT_CAP_256) {
		KRgb f = (attr & KT_A_FGRGB) ? rgb_krgb(cell->fgc)
					     : ktui_theme->slot[cell->fg & 7];
		KRgb b = (attr & KT_A_BGRGB) ? rgb_krgb(cell->bgc)
					     : ktui_theme->slot[cell->bg & 7];

		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";38;5;%d;48;5;%d",
			      rgb_to_256(f), rgb_to_256(b));
	} else if (ktui_caps & KT_CAP_LINUXVT) {
		/* The palette we installed makes slot == ANSI index, so a
		 * literal has nowhere to go: eight colours are all there are
		 * and the slot is the honest one of the two. */
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";%d;%d",
			      30 + (cell->fg & 7), 40 + (cell->bg & 7));
	} else {
		n += snprintf(buf + n, sizeof(buf) - (size_t)n, ";%d;%d",
			      ansi_fg[cell->fg & 7], ansi_bg[cell->bg & 7]);
	}

	n += snprintf(buf + n, sizeof(buf) - (size_t)n, "m");
	if (cacheable && n > 0 && (size_t)n < sizeof(sgr_cache[0])) {
		memcpy(sgr_cache[slot], buf, (size_t)n);
		sgr_len[slot] = (unsigned char)n;
	}
	ktui_term_write(buf, (size_t)n);
}

/* Whether the terminal's current SGR already says what this cell wants. Every
 * field, because a literal that changed with the slot unchanged is a colour
 * change the terminal has not been told about. */
static int same_style(const KtuiCell *a, const KtuiCell *b)
{
	return a->fg == b->fg && a->bg == b->bg && a->attr == b->attr &&
	       a->fgc == b->fgc && a->bgc == b->bgc && a->ulc == b->ulc;
}

/*
 * WHETHER THE TERMINAL IS ALREADY SHOWING THIS CELL, and the literals are part
 * of the answer. emit_sgr() writes `fgc`/`bgc`/`ulc` wherever the matching
 * attribute bit is set, so a cell that moved from one 24-bit colour to another
 * reducing to the same slot keeps every field the slot-only test compares —
 * and the diff would skip a cell whose colour the terminal has not been told
 * about.
 */
static int cell_same(const KtuiCell *a, const KtuiCell *b)
{
	return a->ch == b->ch && same_style(a, b);
}

/*
 * The tty backend's present.
 *
 * Moved here from ktui_draw_flush() UNCHANGED, character for character. Its
 * diff and its emission are one pass — cursor position and SGR state are
 * carried across cells so that only what actually changed is written — and
 * that fusion is why the backend seam hands over both buffers instead of a
 * damage list. Rewriting this to fit a tidier vtable would move pixels on a
 * screen people read.
 */
static void tty_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		      int full)
{
	KtuiCell style;
	int have_style = 0;
	int cx = -1, cy = -1;
	char utf[8];
	/*
	 * SYNCHRONIZED OUTPUT BRACKETS THE WHOLE FRAME, where the terminal
	 * said it understands the mode. A diff frame is a scatter of cursor
	 * moves and single cells, so a terminal drawing as the bytes arrive
	 * shows the new menu before the old one is erased and the row a
	 * scroll moved twice. Held, the frame is shown whole or not at all.
	 */
	int sync = (ktui_caps & KT_CAP_SYNC) != 0;
	/*
	 * THE LINUX VT DRAWS EVERY WIDE CODEPOINT IN ONE COLUMN — its font is
	 * 512 glyphs and none of them is a CJK ideograph — so a pair emitted
	 * there advances the arithmetic below by two while the kernel's cursor
	 * moves one, and every later cell on the row lands a column left with
	 * the diff believing it delivered them. ktui_draw_text substitutes '?'
	 * for the same reason; a cell that reached the frame from anywhere
	 * else is narrowed here instead.
	 */
	int vt = (ktui_caps & KT_CAP_LINUXVT) != 0;

	sgr_check();
	if (sync)
		ktui_term_write("\033[?2026h", 8);

	for (int y = 0; y < h; y++) {
		/* The cell before this one was re-emitted narrow over what had
		 * been a wide glyph, so its continuation is now an orphan the
		 * terminal is still showing half a glyph in. It matches `prev`,
		 * so nothing but this would ever repair it. */
		int redo = 0;

		for (int x = 0; x < w; x++) {
			int i = y * w + x;
			const KtuiCell *b = &cur[i];
			KtuiCell *f = &prev[i];
			int force = redo;
			redo = 0;
			/* A wide glyph and its continuation cell present as one
			 * write that moves the terminal cursor TWO columns, so
			 * they diff and update as a pair — half of one
			 * re-emitted alone would leave cx off by one and every
			 * later cell on the row skipping its reposition. */
			int wide = !vt && b->ch >= 0x80 && x + 1 < w &&
				   cur[i + 1].ch == KTUI_WIDE_CONT &&
				   ktui_wcwidth(b->ch) == 2;
			if (!full && !force && cell_same(b, f)) {
				if (!wide)
					continue;
				if (cell_same(&cur[i + 1], &prev[i + 1])) {
					x++;
					continue;
				}
			}

			if (cy != y || cx != x) {
				ktui_term_printf("\033[%d;%dH", y + 1, x + 1);
				cx = x;
				cy = y;
			}
			if (!have_style || !same_style(b, &style)) {
				emit_sgr(b);
				style = *b;
				have_style = 1;
			}
			/* An orphaned continuation cell — its wide glyph was
			 * overwritten by a narrow one — must not put a control
			 * byte on the wire. */
			uint32_t ch = b->ch == KTUI_WIDE_CONT ? ' '
							      : (b->ch ? b->ch : ' ');
			ch = ktui_sprite_text_cell(ch);
			if (vt && ktui_wcwidth(ch) == 2)
				ch = '?';
			int n = ktui_utf8_encode(ch, utf);
			ktui_term_write(utf, (size_t)n);
			*f = *b;
			if (wide) {
				prev[i + 1] = cur[i + 1];
				cx += 2;
				x++;
			} else {
				cx++;
				redo = x + 1 < w &&
				       cur[i + 1].ch == KTUI_WIDE_CONT;
			}
		}
	}

	ktui_term_write("\033[0m", 4);
	if (sync)
		ktui_term_write("\033[?2026l", 8);
	ktui_term_flush();

	/* The diff above has already recorded this frame as what the screen
	 * shows. If the flush dropped part of it, that belief is wrong for
	 * every cell after the cut, and only a full paint can restore it. */
	if (ktui_term_flush_dropped()) {
		force_full = 1;
		/*
		 * The close went with everything else after the cut, and a
		 * terminal left inside a block shows NOTHING further — the
		 * next frame would be invisible too. So it is written again,
		 * under the same deadline the frame had: a terminal still
		 * refusing eight bytes is one nothing can be written to at
		 * all, and the next frame's own close is what reaches it when
		 * it starts reading again.
		 */
		if (sync) {
			ktui_term_write("\033[?2026l", 8);
			ktui_term_flush();
			(void)ktui_term_flush_dropped();
		}
	}
}

static void tty_size(int *w, int *h)
{
	ktui_term_size_refresh();
	*w = ktui_w;
	*h = ktui_h;
}

static int tty_caps(void)
{
	return ktui_caps;
}

static const KtuiBackend backend_tty = {
	.name = "tty",
	.flush = tty_flush,
	.poll_event = ktui_input_next,
	.size = tty_size,
	.caps = tty_caps,
};

void ktui_backend_set(const KtuiBackend *b)
{
	backend = b ? b : &backend_tty;
	/* A backend change is a different surface with different contents, so
	 * nothing carried over from the old one can be trusted as already
	 * presented. */
	force_full = 1;
}

const KtuiBackend *ktui_backend(void)
{
	return cur_backend();
}

int ktui_offscreen(void)
{
	return offscreen;
}

void ktui_draw_flush(void)
{
	/* Offscreen there is nothing to present to, and stdout is where the
	 * dump goes — a flush would write escape sequences into it. It also has
	 * to leave `back` alone, because the dump reads it. Not hypothetical:
	 * ktui_toosmall() presents itself (kinstall and kdos-appbox return
	 * straight after calling it and never flush), so a preview at a size
	 * below the layout minimum came out as raw SGR.
	 *
	 * Still a latch rather than a backend of its own: `offscreen` also
	 * suppresses the terminal SETUP, which a backend vtable does not
	 * describe, and ktui_draw_dump() reads `back` directly. */
	if (offscreen)
		return;
	/*
	 * EVERY CALL PRESENTS. There is no "nothing changed" shortcut here and
	 * there cannot be one that only watches the cells: a re-registered
	 * animation frame keeps the same sprite key, so its cells are
	 * byte-identical and only its pixels moved, and a gate on the cells
	 * would drop that frame for ever. What a still picture costs is the
	 * backend's own diff, which is where it can be measured and skipped.
	 */
	/*
	 * THE POINTER IS AN OVERLAY, NOT CONTENT, so it goes on for the flush
	 * and comes straight back off. `back` is what the session's cells are
	 * accumulated into and it survives between frames — leaving the
	 * reverse in it would make the pointer a stain the next frame draws
	 * around, one per cell it was ever over.
	 *
	 * Taking it off again is also what erases it: `front` keeps the
	 * reversed cell, `back` does not, so the cell differs and repaints
	 * when the pointer leaves. The same XOR does both jobs.
	 */
	/*
	 * A BACKEND THAT OWNS PIXELS DRAWS AN ARROW INSTEAD, and says so by
	 * answering 1 to `.pointer`. The cell is then left exactly as the
	 * session composed it, because the two pointers would otherwise be
	 * drawn a pixel apart on the same screen.
	 *
	 * IT CANNOT BE DRAWN HERE. This file links nothing but musl and has to
	 * keep doing so — kinstall links it in phase 1, before any library
	 * exists to link against — so an arrow, which needs a pixel buffer and
	 * a colour in it, lives in the backend that already owns both.
	 *
	 * THE HOOK IS CALLED ON EVERY FLUSH, including the ones with no
	 * pointer to report: it is the only thing that tells a backend to take
	 * the last arrow off the screen, and a backend told nothing would
	 * leave one behind at the last place the hand was.
	 *
	 * A BACKEND WITHOUT THE HOOK, OR ONE THAT DECLINES, GETS THE REVERSED
	 * CELL UNCHANGED. That is not a fallback anything may drop: `a11y =
	 * yes` runs this desktop on a --tty view so brltty can read /dev/vcsa,
	 * a dump has no pixels at all, and a view forwarded over ssh is a
	 * terminal on the far end of it. The reversed cell is the pointer on
	 * every one of them.
	 */
	/*
	 * AND OVER AN EMBEDDED GUEST NOTHING IS DRAWN AT ALL — ONE POINTER AT
	 * A TIME.
	 *
	 * A guest's window is a pixel surface somebody else is compositing, and
	 * that compositor renders the cursor into the frames this desktop
	 * pastes in. A reversed cell on top of that is a SECOND pointer a cell
	 * from the first, and the one a person aims a two-pixel scrollbar with
	 * is the guest's.
	 *
	 * THE CELL SAYS SO ITSELF. `KT_A_GUEST` is set by the session on a
	 * guest's content cells and on nothing else, so this is a fact rather
	 * than a guess about what the pixels are doing. A guess made from the
	 * sprite table — counting how often a slot is re-registered — reads a
	 * guest that is merely still as one that is not there: a hand resting
	 * on a window generates no damage, no frame is published, no slot is
	 * re-registered, and the pointer comes back on top of a cursor that is
	 * still on the screen. That is exactly the moment a person clicks.
	 *
	 * A DESKTOP ICON, A PANEL ICON AND AN IMAGE IN A TERMINAL ARE NOT
	 * GUESTS. They are sprites with pixels exactly as a guest's block is,
	 * and nothing is drawing a cursor on them, so they carry no bit and
	 * keep their pointer — which is where it is needed most, on the icon
	 * grid.
	 *
	 * A SPRITE WITH NO PIXELS IS NOT A PICTURE AT ALL. A tty, a view with
	 * no pixel library and a dump all carry the sprite's fallback MARK;
	 * `ktui_sprite_get` answers NULL for a slot with no picture, and the
	 * mark is then set aside for the reverse exactly as a glyph is — the
	 * flush swaps the cell's character for a blank alongside the XOR and
	 * puts it back afterwards, because reverse under a sprite is a fill the
	 * painter composites over and an opaque one hides it completely. It
	 * costs one cell of the picture, which is what a pointer costs over a
	 * glyph too.
	 *
	 * The chrome round an embedded window is drawn in CELLS and carries no
	 * bit, so the pointer comes back the moment it reaches the border —
	 * which is where a window is grabbed, moved and resized. It is never
	 * more than one cell from visible, and that is also the answer for a
	 * guest that has hidden its own cursor, which a player and a game do on
	 * purpose.
	 */
	int pt = -1;
	uint32_t ptch = 0;
	int px = -1, py = -1;

	if (ptr_x >= 0 && ptr_x < bw && ptr_y >= 0 && ptr_y < bh) {
		const KtuiCell *c = &back[ptr_y * bw + ptr_x];
		int slot = KTUI_IS_SPRITE(c->ch)
				   ? (int)KTUI_SPRITE_SLOT(c->ch)
				   : -1;

		/*
		 * AND THE MARK ONLY COUNTS WHERE THE PICTURE IS. The bit says
		 * the session drew this cell from a guest's pixels; it does
		 * NOT say this view has them. A `--tty` view, a view with no
		 * pixel library, a dump and a view over `ssh` all draw the
		 * sprite's fallback MARK instead, and there is no composited
		 * cursor on a mark to collide with — so suppressing the
		 * pointer there would leave that view with no pointer at all
		 * over the one window a person most needs to aim at. That view
		 * is the accessible one: `a11y = yes` runs this desktop on a
		 * `--tty` view precisely so a braille display can read
		 * /dev/vcsa.
		 *
		 * ktui_sprite_get() answers NULL for a slot this view has no
		 * picture for, which is the same question and the only one
		 * that can be asked from here.
		 */
		if (!(c->attr & KT_A_GUEST) || slot < 0 ||
		    !ktui_sprite_get(slot)) {
			px = ptr_x;
			py = ptr_y;
		}
	}

	/* The guest rule is applied BEFORE the hook, so a backend with pixels
	 * and a backend without are told the same thing about the same cell
	 * and neither gets to decide it for itself. */
	if (!(cur_backend()->pointer && cur_backend()->pointer(px, py)) &&
	    px >= 0) {
		pt = py * bw + px;
		back[pt].attr ^= KT_A_REVERSE;
		if (KTUI_IS_SPRITE(back[pt].ch)) {
			ptch = back[pt].ch;
			back[pt].ch = ' ';
		}
	}

	/*
	 * THE FLAG IS TAKEN AND CLEARED BEFORE THE BACKEND RUNS, so that a
	 * backend which asks for a full repaint WHILE it is flushing gets one
	 * on the next frame. Clearing afterwards discards it: the tty backend's
	 * own recovery from a dropped write, and any wrapper that finds its
	 * output cut in half, both set this from inside this call.
	 */
	int full = force_full;

	force_full = 0;
	cur_backend()->flush(back, front, bw, bh, full);
	/*
	 * A BACKEND THAT DECLINED TO PRESENT STILL OWES THE REPAINT. One that
	 * skipped the frame — a display too far behind, a compositor holding
	 * both buffers — left `front` describing a frame the screen never
	 * showed, and the repaint this flush was carrying would be forgotten
	 * with the flag. Asking again is what keeps the two in step.
	 */
	if (full && !force_full && cur_backend()->presented &&
	    !cur_backend()->presented())
		force_full = 1;

	if (pt >= 0) {
		back[pt].attr ^= KT_A_REVERSE;
		if (ptch)
			back[pt].ch = ptch;
	}
}
