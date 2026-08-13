/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   decocheck — is there actually a frame around the window?
 *
 * A SECOND PROCESS, deliberately, and for the same reason traycheck.c is one:
 * a window frame is a conversation between a client and a compositor, and a
 * mock of either side would pass while the real pair failed. This client is its
 * own subject — it maps a window filled with one known colour, asks the
 * compositor for a screenshot through the wlr-screencopy global kdos-comp
 * already implements, finds its own window in the result by that colour, and
 * then looks at what is around it.
 *
 * What it proves, in order:
 *
 *   1. the window is on screen at all, at the size it asked for
 *   2. there is a TITLEBAR above it — a band exactly one cell tall whose
 *      pixels are neither the window's colour nor the desktop's
 *   3. there are left, right and bottom borders of the same thickness
 *   4. there is a SHADOW down and to the right, and it is darker than the
 *      desktop it falls on
 *   5. unfocusing the window CHANGES the titlebar — which is the whole of the
 *      double-line/single-line claim, checked as pixels rather than as intent
 *
 * (5) is the one worth having. Focus weight is the sort of thing that is
 * obviously right in the source and silently wrong on screen, because the two
 * glyph sets are the same width and a mistake moves nothing.
 * ---------------------------------
 */

/* Guarded, because the suite already passes -D_GNU_SOURCE and -Werror turns a
 * redefinition into a build failure. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

#include "wlr-screencopy-unstable-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define WIN_W 400
#define WIN_H 300
/* Pure green, opaque. Chosen because nothing else on this desktop is it: the
 * phosphor accent is #39ff14 and the surface colours are near-black, so a
 * search for exactly this value cannot land on the compositor's own chrome. */
#define WIN_ARGB 0xff00ff00u

static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wm_base;
static struct zwlr_screencopy_manager_v1 *screencopy;
static struct wl_output *output;
/*
 * xdg-decoration, and binding it is not cosmetic.
 *
 * The compositor's frame code has a whole branch that only runs when a client
 * creates a decoration object and asks for a mode — and the first version of
 * this test never did, so that branch was never exercised here and crashed the
 * compositor on the first real window on the ISO. A test client that is easier
 * on the compositor than a real one is not testing the compositor.
 */
static struct zxdg_decoration_manager_v1 *deco_mgr;

static int fail_count;

static void check(bool ok, const char *what)
{
	printf("  %-58s %s\n", what, ok ? "ok" : "FAIL");
	if (!ok)
		fail_count++;
}

/* ── shm ───────────────────────────────────────────────────────────────── */

static int anon_fd(size_t size)
{
	int fd = memfd_create("decocheck", MFD_CLOEXEC);
	if (fd < 0)
		return -1;
	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

struct shmbuf {
	struct wl_buffer *wl;
	uint8_t *data;
	int w, h, stride, bpp;
	uint32_t format;
	size_t size;
};

/*
 * The FORMAT IS THE COMPOSITOR'S CHOICE, never ours.
 *
 * wlr-screencopy names a format and a stride in its `buffer` event and rejects
 * anything else with `invalid buffer format`. Hardcoding ARGB8888 is exactly
 * how this test failed first time round, and it is the same trap CLAUDE.md
 * already records against the headless GLES2 path, which advertises BGR888 at
 * three bytes per pixel — a client that assumes four gets `invalid stride`.
 */
static int format_bpp(uint32_t fmt)
{
	switch (fmt) {
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_XRGB8888:
	case WL_SHM_FORMAT_ABGR8888:
	case WL_SHM_FORMAT_XBGR8888:
		return 4;
	case WL_SHM_FORMAT_BGR888:
	case WL_SHM_FORMAT_RGB888:
		return 3;
	default:
		return 0;
	}
}

/* Everything above this line works in the compositor's layout; everything below
 * it works in 0xAARRGGBB. This is the one place that converts. */
static uint32_t decode(const uint8_t *p, uint32_t fmt)
{
	switch (fmt) {
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_XRGB8888:
		return 0xff000000u | ((uint32_t)p[2] << 16) |
		       ((uint32_t)p[1] << 8) | p[0];
	case WL_SHM_FORMAT_ABGR8888:
	case WL_SHM_FORMAT_XBGR8888:
		return 0xff000000u | ((uint32_t)p[0] << 16) |
		       ((uint32_t)p[1] << 8) | p[2];
	case WL_SHM_FORMAT_BGR888:
		return 0xff000000u | ((uint32_t)p[2] << 16) |
		       ((uint32_t)p[1] << 8) | p[0];
	case WL_SHM_FORMAT_RGB888:
		return 0xff000000u | ((uint32_t)p[0] << 16) |
		       ((uint32_t)p[1] << 8) | p[2];
	default:
		return 0;
	}
}

static void encode(uint8_t *p, uint32_t fmt, uint32_t argb)
{
	uint8_t r = (argb >> 16) & 0xff, g = (argb >> 8) & 0xff, b = argb & 0xff;
	switch (fmt) {
	case WL_SHM_FORMAT_ARGB8888:
	case WL_SHM_FORMAT_XRGB8888:
		p[0] = b; p[1] = g; p[2] = r; p[3] = 0xff;
		break;
	case WL_SHM_FORMAT_ABGR8888:
	case WL_SHM_FORMAT_XBGR8888:
		p[0] = r; p[1] = g; p[2] = b; p[3] = 0xff;
		break;
	case WL_SHM_FORMAT_BGR888:
		p[0] = b; p[1] = g; p[2] = r;
		break;
	case WL_SHM_FORMAT_RGB888:
		p[0] = r; p[1] = g; p[2] = b;
		break;
	default:
		break;
	}
}

static bool shmbuf_make(struct shmbuf *b, int w, int h, uint32_t fmt, int stride)
{
	b->bpp = format_bpp(fmt);
	if (!b->bpp)
		return false;
	b->w = w;
	b->h = h;
	b->format = fmt;
	b->stride = stride > 0 ? stride : w * b->bpp;
	b->size = (size_t)b->stride * h;

	int fd = anon_fd(b->size);
	if (fd < 0)
		return false;
	b->data = mmap(NULL, b->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (b->data == MAP_FAILED) {
		close(fd);
		return false;
	}
	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)b->size);
	b->wl = wl_shm_pool_create_buffer(pool, 0, w, h, b->stride, fmt);
	wl_shm_pool_destroy(pool);
	close(fd);
	return b->wl != NULL;
}

/* ── registry ──────────────────────────────────────────────────────────── */

static void wm_base_ping(void *d, struct xdg_wm_base *b, uint32_t serial)
{
	(void)d;
	xdg_wm_base_pong(b, serial);
}
static const struct xdg_wm_base_listener wm_base_listener = { wm_base_ping };

static void reg_global(void *data, struct wl_registry *reg, uint32_t name,
		       const char *iface, uint32_t version)
{
	(void)data;
	(void)version;
	if (!strcmp(iface, wl_compositor_interface.name))
		compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, wl_shm_interface.name))
		shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
	else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
	} else if (!strcmp(iface, zwlr_screencopy_manager_v1_interface.name))
		screencopy = wl_registry_bind(reg, name,
					      &zwlr_screencopy_manager_v1_interface, 1);
	else if (!strcmp(iface, zxdg_decoration_manager_v1_interface.name))
		deco_mgr = wl_registry_bind(reg, name,
					    &zxdg_decoration_manager_v1_interface, 1);
	else if (!strcmp(iface, wl_output_interface.name) && !output)
		output = wl_registry_bind(reg, name, &wl_output_interface, 1);
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t n)
{
	(void)d; (void)r; (void)n;
}
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

/* ── a window ──────────────────────────────────────────────────────────── */

struct win {
	struct wl_surface *surface;
	struct xdg_surface *xdg;
	struct xdg_toplevel *toplevel;
	struct shmbuf buf;
	bool configured;
	uint32_t argb;
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg,
				  uint32_t serial)
{
	struct win *w = data;
	xdg_surface_ack_configure(xdg, serial);
	w->configured = true;
}
static const struct xdg_surface_listener xdg_surface_listener = {
	xdg_surface_configure
};

static void tl_configure(void *d, struct xdg_toplevel *t, int32_t w, int32_t h,
			 struct wl_array *states)
{
	(void)d; (void)t; (void)w; (void)h; (void)states;
}
static void tl_close(void *d, struct xdg_toplevel *t) { (void)d; (void)t; }
static const struct xdg_toplevel_listener tl_listener = {
	tl_configure, tl_close, NULL, NULL
};

static bool win_open(struct win *w, struct wl_display *dpy, const char *title,
		     uint32_t argb)
{
	w->argb = argb;
	w->surface = wl_compositor_create_surface(compositor);
	w->xdg = xdg_wm_base_get_xdg_surface(wm_base, w->surface);
	xdg_surface_add_listener(w->xdg, &xdg_surface_listener, w);
	w->toplevel = xdg_surface_get_toplevel(w->xdg);
	xdg_toplevel_add_listener(w->toplevel, &tl_listener, w);
	xdg_toplevel_set_title(w->toplevel, title);
	xdg_toplevel_set_app_id(w->toplevel, "decocheck");
	if (deco_mgr) {
		/* Ask for server-side, which is what a client that wants the
		 * compositor's frame does — and what foot does. */
		struct zxdg_toplevel_decoration_v1 *d =
			zxdg_decoration_manager_v1_get_toplevel_decoration(
				deco_mgr, w->toplevel);
		zxdg_toplevel_decoration_v1_set_mode(d,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
	wl_surface_commit(w->surface);

	while (!w->configured)
		if (wl_display_dispatch(dpy) < 0)
			return false;

	/* The window's own buffer is ours to choose, so ARGB8888 — every
	 * compositor supports it for wl_shm. Only the SCREENSHOT has to take
	 * the format it is given. */
	if (!shmbuf_make(&w->buf, WIN_W, WIN_H, WL_SHM_FORMAT_ARGB8888, 0))
		return false;
	for (int y = 0; y < WIN_H; y++)
		for (int x = 0; x < WIN_W; x++)
			encode(w->buf.data + (size_t)y * w->buf.stride + x * 4,
			       WL_SHM_FORMAT_ARGB8888, argb);
	wl_surface_attach(w->surface, w->buf.wl, 0, 0);
	wl_surface_damage_buffer(w->surface, 0, 0, WIN_W, WIN_H);
	wl_surface_commit(w->surface);
	wl_display_roundtrip(dpy);
	return true;
}

/* ── screencopy ────────────────────────────────────────────────────────── */

struct shot {
	struct shmbuf buf;
	int w, h;
	bool ready, failed;
	uint32_t format;
	int stride;
};

static void sc_buffer(void *data, struct zwlr_screencopy_frame_v1 *f,
		      uint32_t format, uint32_t w, uint32_t h, uint32_t stride)
{
	struct shot *s = data;
	s->w = (int)w;
	s->h = (int)h;
	s->format = format;
	s->stride = (int)stride;
	if (!shmbuf_make(&s->buf, (int)w, (int)h, format, (int)stride)) {
		fprintf(stderr, "decocheck: cannot take format 0x%08x\n", format);
		s->failed = true;
		return;
	}
	zwlr_screencopy_frame_v1_copy(f, s->buf.wl);
}
static void sc_flags(void *d, struct zwlr_screencopy_frame_v1 *f, uint32_t fl)
{
	(void)d; (void)f; (void)fl;
}
static void sc_ready(void *data, struct zwlr_screencopy_frame_v1 *f,
		     uint32_t hi, uint32_t lo, uint32_t nsec)
{
	struct shot *s = data;
	(void)f; (void)hi; (void)lo; (void)nsec;
	s->ready = true;
}
static void sc_failed(void *data, struct zwlr_screencopy_frame_v1 *f)
{
	struct shot *s = data;
	(void)f;
	s->failed = true;
}
static const struct zwlr_screencopy_frame_v1_listener sc_listener = {
	sc_buffer, sc_flags, sc_ready, sc_failed, NULL, NULL
};

static bool grab(struct wl_display *dpy, struct shot *s)
{
	memset(s, 0, sizeof(*s));
	struct zwlr_screencopy_frame_v1 *f =
		zwlr_screencopy_manager_v1_capture_output(screencopy, 0, output);
	zwlr_screencopy_frame_v1_add_listener(f, &sc_listener, s);
	while (!s->ready && !s->failed)
		if (wl_display_dispatch(dpy) < 0)
			return false;
	zwlr_screencopy_frame_v1_destroy(f);
	return s->ready;
}

/* ── looking at the result ─────────────────────────────────────────────── */

static uint32_t px(struct shot *s, int x, int y)
{
	if (x < 0 || y < 0 || x >= s->w || y >= s->h)
		return 0;
	return decode(s->buf.data + (size_t)y * s->buf.stride +
		      (size_t)x * s->buf.bpp, s->buf.format);
}

/* The window is the only run of this exact colour. */
static bool find_window(struct shot *s, uint32_t argb, int *ox, int *oy)
{
	for (int y = 0; y < s->h; y++)
		for (int x = 0; x < s->w; x++)
			if (px(s, x, y) == argb) {
				*ox = x;
				*oy = y;
				return true;
			}
	return false;
}

static int luma(uint32_t p)
{
	return (int)((((p >> 16) & 0xff) * 30 + ((p >> 8) & 0xff) * 59 +
		      (p & 0xff) * 11) / 100);
}

static void dump_ppm(const char *path, struct shot *s)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
	for (int y = 0; y < s->h; y++)
		for (int x = 0; x < s->w; x++) {
			uint32_t p = px(s, x, y);
			unsigned char rgb[3] = { (p >> 16) & 0xff,
						 (p >> 8) & 0xff, p & 0xff };
			fwrite(rgb, 3, 1, f);
		}
	fclose(f);
}

/* How many DISTINCT non-window colours appear on the row `dy` above the
 * window's top edge. A titlebar has at least the rule, the text and the
 * background; an empty band has one. */
static int distinct_on_row(struct shot *s, int x0, int x1, int y, uint32_t skip)
{
	uint32_t seen[32];
	int n = 0;
	for (int x = x0; x < x1; x++) {
		uint32_t p = px(s, x, y);
		if (p == skip)
			continue;
		bool have = false;
		for (int i = 0; i < n; i++)
			if (seen[i] == p)
				have = true;
		if (!have && n < 32)
			seen[n++] = p;
	}
	return n;
}

int main(void)
{
	struct wl_display *dpy = wl_display_connect(NULL);
	if (!dpy) {
		fprintf(stderr, "decocheck: no compositor\n");
		return 2;
	}
	struct wl_registry *reg = wl_display_get_registry(dpy);
	wl_registry_add_listener(reg, &reg_listener, NULL);
	wl_display_roundtrip(dpy);

	if (!compositor || !shm || !wm_base || !screencopy || !output) {
		fprintf(stderr, "decocheck: missing globals "
			"(compositor=%p shm=%p xdg=%p screencopy=%p output=%p)\n",
			(void *)compositor, (void *)shm, (void *)wm_base,
			(void *)screencopy, (void *)output);
		return 2;
	}

	printf("decocheck — window frames\n");

	struct win w1;
	memset(&w1, 0, sizeof(w1));
	if (!win_open(&w1, dpy, "DECOCHECK ONE", WIN_ARGB)) {
		fprintf(stderr, "decocheck: could not map a window\n");
		return 2;
	}
	/* Two roundtrips: the frame is painted from the commit handler, so the
	 * first one only guarantees the compositor has SEEN the window. */
	wl_display_roundtrip(dpy);
	wl_display_roundtrip(dpy);

	struct shot s;
	if (!grab(dpy, &s)) {
		fprintf(stderr, "decocheck: screencopy failed\n");
		return 2;
	}
	dump_ppm("/tmp/decocheck-focused.ppm", &s);

	int wx, wy;
	bool found = find_window(&s, WIN_ARGB, &wx, &wy);
	check(found, "the window is on screen");
	if (!found) {
		printf("\n%d failed\n", fail_count);
		return 1;
	}

	/* Measure the window's extent from the found corner. */
	int right = wx;
	while (px(&s, right + 1, wy) == WIN_ARGB)
		right++;
	int bottom = wy;
	while (px(&s, wx, bottom + 1) == WIN_ARGB)
		bottom++;
	int ww = right - wx + 1, wh = bottom - wy + 1;
	printf("  window at %d,%d  %dx%d   screen %dx%d\n", wx, wy, ww, wh,
	       s.w, s.h);
	check(ww == WIN_W && wh == WIN_H, "it is the size it asked for");

	/* The titlebar: the band above the window. Its height is the cell
	 * height, which this client does not know — so measure it by walking up
	 * until the pixels stop differing from the desktop behind. */
	uint32_t desktop = px(&s, 2, 2);
	int band = 0;
	while (wy - band - 1 >= 0 && band < 200 &&
	       px(&s, wx + ww / 2, wy - band - 1) != desktop)
		band++;
	printf("  titlebar band %d px, desktop %06x\n", band,
	       desktop & 0xffffff);
	check(band > 0, "there is a band above the window");

	int distinct = distinct_on_row(&s, wx, wx + ww, wy - band / 2, WIN_ARGB);
	check(distinct >= 2,
	      "the titlebar has a rule and text, not one flat colour");

	/* Left, right and bottom borders, the same thickness as the top. */
	check(px(&s, wx - 1, wy + wh / 2) != desktop &&
	      px(&s, wx - 1, wy + wh / 2) != WIN_ARGB,
	      "there is a left border");
	check(px(&s, wx + ww, wy + wh / 2) != desktop &&
	      px(&s, wx + ww, wy + wh / 2) != WIN_ARGB,
	      "there is a right border");
	check(px(&s, wx + ww / 2, wy + wh) != desktop &&
	      px(&s, wx + ww / 2, wy + wh) != WIN_ARGB,
	      "there is a bottom border");

	/*
	 * The shadow. It is down and to the RIGHT of the frame, so a point just
	 * past the right border and below the top of the frame is in it — and it
	 * must be DARKER than the desktop it falls on, because that is what a
	 * shadow is. Comparing luma rather than equality: the shadow is a 50%
	 * black rect over whatever was underneath, not a colour of its own.
	 */
	int shx = wx + ww + band + 2;
	int shy = wy + wh / 2;
	int lum_shadow = luma(px(&s, shx, shy));
	int lum_desktop = luma(desktop);
	printf("  shadow luma %d  desktop luma %d\n", lum_shadow, lum_desktop);
	check(shx < s.w && lum_shadow <= lum_desktop,
	      "there is a shadow, and it is darker than the desktop");

	/*
	 * Focus weight. A second window takes the focus, so the first is
	 * repainted with a single-line frame in the dim colour. The claim is
	 * about pixels, so it is checked as pixels.
	 */
	uint32_t before[64];
	int nb = 0;
	for (int x = wx; x < wx + ww && nb < 64; x += ww / 64 + 1)
		before[nb++] = px(&s, x, wy - band / 2);

	struct win w2;
	memset(&w2, 0, sizeof(w2));
	bool two = win_open(&w2, dpy, "DECOCHECK TWO", 0xff0000ffu);
	check(two, "a second window opens and takes the focus");
	wl_display_roundtrip(dpy);
	wl_display_roundtrip(dpy);

	struct shot s2;
	if (two && grab(dpy, &s2)) {
		dump_ppm("/tmp/decocheck-unfocused.ppm", &s2);
		int changed = 0;
		for (int i = 0, x = wx; i < nb; i++, x += ww / 64 + 1)
			if (px(&s2, x, wy - band / 2) != before[i])
				changed++;
		printf("  %d of %d titlebar samples changed on unfocus\n",
		       changed, nb);
		check(changed > 0,
		      "unfocusing the first window repaints its titlebar");
	} else {
		check(false, "second screenshot");
	}

	printf("\n%s (%d failed)\n", fail_count ? "FAILED" : "all ok",
	       fail_count);
	wl_display_disconnect(dpy);
	return fail_count ? 1 : 0;
}
