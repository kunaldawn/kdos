/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-theme — the cursor theme
 *
 * art/ holds monochrome Xcursor shapes (white body, black outline, grey
 * anti-aliasing), pruned by vendor.py from a Bibata-Modern-Ice release. This
 * paints them in the KDOS palette and lays out the theme:
 *
 *   * Every pixel's luminance is mapped onto a ramp from the scheme's variant
 *     (the outline) to its primary (the body), so the artwork stops being
 *     greyscale and starts being KDOS.
 *   * Busy shapes (wait, progress) ramp to the SECONDARY instead — the same
 *     "working on it" colour the boot splash uses for a pending stage.
 *   * Xcursor ARGB is PREMULTIPLIED: alpha is divided out before the ramp and
 *     multiplied back after, or every anti-aliased edge becomes a dark halo.
 *   * The real file of each alias group is the CSS name and every X11 name is
 *     a symlink to it. That direction matters on upgrades — the theme this
 *     replaced was laid out the same way, so a stale `left_ptr -> default` is
 *     overwritten by an identical link instead of forming a loop that makes
 *     kpkg abort with "Symbolic link loop".
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kdos-theme.h"

#define IMAGE_TYPE 0xFFFD0002u

/* Canonical shape -> X11/legacy names that must resolve to it. Keep in sync
 * with SHAPES in vendor.py; anything listed here needs art/<shape> to exist. */
static const struct {
	const char *shape;
	const char *aliases[9];
} ALIASES[] = {
	{ "alias", { "dnd-link", "link", NULL } },
	{ "cell", { "plus", NULL } },
	{ "context-menu", { NULL } },
	{ "copy", { "dnd-copy", "1081e37283d90000800003c07f3ef6bf",
		    "6407b0e94181790501fd1e167b474872",
		    "b66166c04f8c3109214a4fbd64a50fc8", NULL } },
	{ "crosshair", { NULL } },
	{ "default", { "arrow", "left_ptr", "top_left_arrow", NULL } },
	{ "dnd-move", { "closedhand", "dnd-none", "grabbing",
			"fcf21c00b30f7e3f83fe0dfd12e71cff", NULL } },
	{ "e-resize", { "right_side", NULL } },
	{ "ew-resize", { "col-resize", "h_double_arrow", "sb_h_double_arrow",
			 "size_hor", "split_h",
			 "028006030e0e7ebffc7f7070c0600140",
			 "14fef782d02440884392942c1120523", NULL } },
	{ "grab", { "hand1", "openhand", NULL } },
	{ "help", { "left_ptr_help", "question_arrow", "whats_this",
		    "5c6cd98b3f3ebcb1f9c7f1c204630408",
		    "d9ce0ab605698f320427677b458ad60b", NULL } },
	{ "move", { "all-scroll", "fleur", "size_all",
		    "4498f0e0c1937ffe01fd06f973665830",
		    "9081237383d90e509aa00f00170e968f", NULL } },
	{ "n-resize", { "top_side", NULL } },
	{ "ne-resize", { "top_right_corner", NULL } },
	{ "nesw-resize", { "fd_double_arrow", "size_bdiag",
			   "fcf1c3c7cd4491d801f1e1c78f100000", NULL } },
	{ "no-drop", { "dnd_no_drop", NULL } },
	{ "not-allowed", { "crossed_circle", "forbidden",
			   "03b6e0fcb3499374a867c041f52298f0", NULL } },
	{ "ns-resize", { "row-resize", "v_double_arrow", "sb_v_double_arrow",
			 "size_ver", "split_v", "double_arrow",
			 "00008160000006810000408080010102",
			 "2870a09082c103050810ffdffffe0204", NULL } },
	{ "nw-resize", { "top_left_corner", NULL } },
	{ "nwse-resize", { "bd_double_arrow", "size_fdiag",
			   "c7088f0f3e6c8088236ef8e1e3e70000", NULL } },
	{ "pointer", { "hand2", "pointing_hand",
		       "9d800788f1b08800ae810202380a0822",
		       "e29285e634086352946a0e7090d73106", NULL } },
	{ "progress", { "left_ptr_watch", "00000000000000020006000e7e9ffc3f",
			"08e8e1c95fe2fc01f976f1e063a24ccd",
			"3ecb610c1bf2410f44200f48c40d3599", NULL } },
	{ "s-resize", { "bottom_side", NULL } },
	{ "se-resize", { "bottom_right_corner", NULL } },
	{ "sw-resize", { "bottom_left_corner", NULL } },
	{ "text", { "ibeam", "xterm", NULL } },
	{ "vertical-text", { NULL } },
	{ "w-resize", { "left_side", NULL } },
	{ "wait", { "watch", NULL } },
	{ "wayland-cursor", { NULL } },
	{ "zoom-in", { NULL } },
	{ "zoom-out", { NULL } },
};
#define NSHAPE ((int)(sizeof(ALIASES) / sizeof(ALIASES[0])))

static int is_busy(const char *shape)
{
	return !strcmp(shape, "wait") || !strcmp(shape, "progress");
}

/* ──────────────────────────────────────────────────────────────────────── */

typedef uint8_t Lut[256][3];

/* python's int(round(x)) — half to even. */
static int rnd(double x)
{
	long long i = (long long)x;
	double frac = x - (double)i;
	if (frac > 0.5 || (frac == 0.5 && (i & 1)))
		i++;
	return (int)i;
}

static void build_lut(Lut lut, uint32_t outline, uint32_t top)
{
	int o[3] = { (int)((outline >> 16) & 0xff), (int)((outline >> 8) & 0xff),
		     (int)(outline & 0xff) };
	int t[3] = { (int)((top >> 16) & 0xff), (int)((top >> 8) & 0xff),
		     (int)(top & 0xff) };
	for (int i = 0; i < 256; i++)
		for (int c = 0; c < 3; c++)
			lut[i][c] = (uint8_t)rnd(o[c] + (t[c] - o[c]) *
							(i / 255.0));
}

static uint32_t rd32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	       (uint32_t)p[3] << 24;
}

static void wr32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}

static void recolor(unsigned char *dst, const unsigned char *src, size_t count,
		    const Lut lut)
{
	for (size_t i = 0; i < count; i++) {
		uint32_t v = rd32(src + i * 4);
		unsigned a = (v >> 24) & 0xff;
		if (!a) {
			/* A fully transparent pixel keeps no colour at all —
			 * the destination stays zeroed, which is what the
			 * bytearray this replaces did. */
			wr32(dst + i * 4, 0);
			continue;
		}
		unsigned r = (v >> 16) & 0xff, g = (v >> 8) & 0xff, b = v & 0xff;
		if (a != 0xff) {
			r = r * 255 / a;
			g = g * 255 / a;
			b = b * 255 / a;
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
		}
		unsigned idx = (r * 299 + g * 587 + b * 114) / 1000;
		if (idx > 255)
			idx = 255;
		unsigned nr = lut[idx][0], ng = lut[idx][1], nb = lut[idx][2];
		if (a != 0xff) {
			nr = nr * a / 255;
			ng = ng * a / 255;
			nb = nb * a / 255;
		}
		wr32(dst + i * 4, (uint32_t)a << 24 | nr << 16 | ng << 8 | nb);
	}
}

static int convert(const char *src, const char *dst, const Lut lut)
{
	size_t len = 0;
	unsigned char *d = (unsigned char *)kb_read_all(src, &len);
	if (!d)
		kb_die("cannot read %s", src);
	if (len < 16 || memcmp(d, "Xcur", 4))
		kb_die("%s is not an Xcursor file", src);

	uint32_t ntoc = rd32(d + 12);
	if (16 + (size_t)ntoc * 12 > len)
		kb_die("%s has a truncated table of contents", src);

	KbBuf toc = {0}, body = {0};
	uint32_t pos = 16 + ntoc * 12;

	for (uint32_t i = 0; i < ntoc; i++) {
		const unsigned char *e = d + 16 + i * 12;
		uint32_t ctype = rd32(e), subtype = rd32(e + 4), off = rd32(e + 8);
		unsigned char hdr[12];
		wr32(hdr, ctype);
		wr32(hdr + 4, subtype);
		wr32(hdr + 8, pos);
		kb_buf_add(&toc, hdr, sizeof(hdr));

		if (ctype == IMAGE_TYPE) {
			if (off + 36 > len)
				kb_die("%s: image chunk past end", src);
			uint32_t w = rd32(d + off + 16), h = rd32(d + off + 20);
			size_t px = (size_t)w * h;
			if (off + 36 + px * 4 > len)
				kb_die("%s: image payload past end", src);
			unsigned char *buf = kb_calloc(1, 36 + px * 4);
			memcpy(buf, d + off, 36);
			recolor(buf + 36, d + off + 36, px, lut);
			kb_buf_add(&body, buf, 36 + px * 4);
			pos += (uint32_t)(36 + px * 4);
			free(buf);
		} else {
			if (off + 4 > len)
				kb_die("%s: chunk past end", src);
			uint32_t clen = rd32(d + off);
			if (off + clen > len)
				kb_die("%s: chunk payload past end", src);
			kb_buf_add(&body, d + off, clen);
			pos += clen;
		}
	}

	KbBuf out = {0};
	unsigned char head[16];
	memcpy(head, "Xcur", 4);
	wr32(head + 4, 16);
	wr32(head + 8, 0x10000);
	wr32(head + 12, ntoc);
	kb_buf_add(&out, head, sizeof(head));
	kb_buf_add(&out, toc.p, toc.n);
	kb_buf_add(&out, body.p, body.n);

	int rc = kb_write_all(dst, out.p, out.n);

	free(d);
	kb_buf_free(&toc);
	kb_buf_free(&body);
	kb_buf_free(&out);
	return rc;
}

/* ──────────────────────────────────────────────────────────────────────── */

int gen_cursors(const char *art, const char *out, const KcolScheme *sc)
{
	if (!kb_is_dir(art))
		kb_die("no cursor artwork at %s", art);

	char *curdir = kb_path_join(out, "cursors");
	kb_mkdir_p(curdir);

	Lut body_lut, busy_lut;
	build_lut(body_lut, sc->variant, sc->primary);
	build_lut(busy_lut, sc->variant, sc->secondary);

	int shapes = 0, links = 0;
	for (int i = 0; i < NSHAPE; i++) {
		const char *shape = ALIASES[i].shape;
		char *src = kb_path_join(art, shape);
		if (!kb_path_exists(src))
			kb_die("art/%s is missing — re-run the vendor step", shape);
		char *dst = kb_path_join(curdir, shape);
		convert(src, dst, is_busy(shape) ? busy_lut : body_lut);
		shapes++;
		free(src);
		free(dst);

		for (int a = 0; ALIASES[i].aliases[a]; a++) {
			char *link = kb_path_join(curdir, ALIASES[i].aliases[a]);
			unlink(link);
			if (symlink(shape, link) == 0)
				links++;
			free(link);
		}
	}

	char *p = kb_path_join(out, "index.theme");
	static const char IDX[] = "[Icon Theme]\n"
				  "Name=KDOS-cursors\n"
				  "Comment=KDOS phosphor cursors\n"
				  "Inherits=\n";
	kb_write_all(p, IDX, sizeof(IDX) - 1);
	free(p);

	p = kb_path_join(out, "cursor.theme");
	static const char CUR[] = "[Icon Theme]\nName=KDOS-cursors\n"
				  "Inherits=KDOS-cursors\n";
	kb_write_all(p, CUR, sizeof(CUR) - 1);
	free(p);

	printf("kdos-cursors: %d shapes, %d aliases -> %s\n", shapes, links,
	       curdir);
	free(curdir);
	return 0;
}
