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
	int cur_fg = -1, cur_bg = -1, cur_attr = -1;
	int cx = -1, cy = -1;
	char utf[8];

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
			int wide = b->ch >= 0x80 && x + 1 < w &&
				   cur[i + 1].ch == KTUI_WIDE_CONT &&
				   ktui_wcwidth(b->ch) == 2;
			if (!full && !force && b->ch == f->ch &&
			    b->fg == f->fg && b->bg == f->bg &&
			    b->attr == f->attr) {
				if (!wide)
					continue;
				const KtuiCell *b2 = &cur[i + 1];
				const KtuiCell *f2 = &prev[i + 1];
				if (b2->ch == f2->ch && b2->fg == f2->fg &&
				    b2->bg == f2->bg && b2->attr == f2->attr) {
					x++;
					continue;
				}
			}

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
			/* An orphaned continuation cell — its wide glyph was
			 * overwritten by a narrow one — must not put a control
			 * byte on the wire. */
			uint32_t ch = b->ch == KTUI_WIDE_CONT ? ' '
							      : (b->ch ? b->ch : ' ');
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
	ktui_term_flush();
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
	if (ptr_x >= 0 && ptr_x < bw && ptr_y >= 0 && ptr_y < bh)
		back[ptr_y * bw + ptr_x].attr ^= KT_A_REVERSE;

	cur_backend()->flush(back, front, bw, bh, force_full);
	force_full = 0;
}
