/* libkkms — the seat, the mode and the framebuffer. See kkms.h. */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <libseat.h>
#include <pixman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "kcell.h"
#include "kkms.h"
#include "kkms_priv.h"

struct kkms K;

/* ── why the screen was not taken ─────────────────────────────────────────
 *
 * KEPT OUTSIDE `K`. kkms_init() clears that struct on entry and its failure
 * path runs kkms_shutdown(), so a reason stored there is erased by the cleanup
 * that follows the failure it describes. A caller reads this after -1.
 *
 * Eight steps can fail and one return value carries all of them, so a
 * supervisor that only sees -1 learns nothing it can act on: a missing driver,
 * a seat that never went active, a monitor that is not plugged in and a
 * modeset the driver rejected each want a different answer from whoever is
 * standing at the machine.
 * ──────────────────────────────────────────────────────────────────────── */

static char reason[192];

static int fail_with(const char *step, const char *detail)
{
	if (detail && *detail)
		snprintf(reason, sizeof(reason), "%s: %s", step, detail);
	else
		snprintf(reason, sizeof(reason), "%s", step);
	return -1;
}

const char *kkms_reason(void)
{
	return reason[0] ? reason : "no failure recorded";
}

/* ── the seat ────────────────────────────────────────────────────────────
 *
 * Enable and disable are a VT switch. On disable every device this holds is
 * suspended by the kernel and the framebuffer belongs to somebody else, so
 * nothing is drawn until enable comes back — and then everything is, because
 * whatever had the screen left it in an unknown state.
 * ──────────────────────────────────────────────────────────────────────── */

static void on_enable(struct libseat *seat, void *data)
{
	(void)seat;
	(void)data;
	K.active = 1;

	/* The mode has to be set again: the VT we came back to was somebody
	 * else's and its CRTC is theirs. */
	/* EVERY screen, not the primary alone: the VT we came back to had all
	 * of them and left every CRTC in an unknown state. */
	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		if (K.drm_fd >= 0 && o->crtc && o->fb)
			drmModeSetCrtc(K.drm_fd, o->crtc, o->fb, 0, 0,
				       &o->connector, 1, &o->mode);
		o->force_full = 1;
	}
}

static void on_disable(struct libseat *seat, void *data)
{
	(void)data;
	K.active = 0;
	/* Acknowledged immediately: the switch does not complete until it is,
	 * and a session that sits on it wedges the machine's VT switching. */
	libseat_disable_seat(seat);
}

static struct libseat_seat_listener seat_listener = {
	.enable_seat = on_enable,
	.disable_seat = on_disable,
};

/* ── the device ──────────────────────────────────────────────────────── */

static int open_drm(const char *card)
{
	/*
	 * The first card with a connected output wins. Enumerated rather than
	 * assumed: card0 is whichever device the kernel probed first, which on
	 * a machine with a discrete card and an integrated one is not
	 * necessarily the one with a screen on it.
	 */
	int opened = 0;

	/*
	 * A NAMED DEVICE IS TRIED AND NOTHING ELSE IS. A caller that said
	 * which card it meant wants that card's failure, not a quiet fallback
	 * onto whichever other one happens to have a screen — which on a
	 * machine with an emulated display and a virtual one is the emulated
	 * one every time.
	 */
	for (int i = 0; i < (card ? 1 : 8); i++) {
		char path[64];

		if (card)
			snprintf(path, sizeof(path), "%s", card);
		else
			snprintf(path, sizeof(path), "/dev/dri/card%d", i);

		int fd = -1;
		int id = libseat_open_device(K.seat, path, &fd);

		if (id < 0 || fd < 0)
			continue;

		opened++;

		drmModeRes *res = drmModeGetResources(fd);

		if (res && res->count_connectors > 0) {
			K.drm_fd = fd;
			K.drm_dev = id;
			K.res = res;
			return 0;
		}

		if (res)
			drmModeFreeResources(res);
		libseat_close_device(K.seat, id);
	}

	if (card)
		return fail_with("open_drm",
				 opened ? "the named card reports no connectors"
					: "the seat would not open the named card");
	return fail_with("open_drm",
			 opened ? "a DRM device opened and reports no connectors"
				: "the seat opened no /dev/dri/card0..7");
}

/*
 * EVERY CONNECTED CONNECTOR, IN CONNECTOR ORDER.
 *
 * The order is the DRM resource list's, which is the kernel's own and is
 * stable across a boot — a layout that depended on which screen answered first
 * would rearrange the desktop depending on how fast a monitor woke up. Screens
 * are then placed edge to edge from the left in that order: an ORDER and not a
 * geometry, exactly as the window model already says.
 *
 * A CRTC IS TAKEN ONCE. Two connectors whose encoders both offer the same CRTC
 * would otherwise be given it twice, and the second modeset takes the screen
 * away from the first — a two-monitor machine that lights one.
 */
/*
 * THE KIND OF SOCKET A CONNECTOR IS, as a word.
 *
 * libdrm publishes the numbers and not the names, and every tool that shows a
 * screen builds the same string from them: the type plus the per-type index is
 * what a monitor is labelled with everywhere a person has seen one.
 */
static const char *conn_type_name(uint32_t t)
{
	switch (t) {
	case DRM_MODE_CONNECTOR_HDMIA:		return "HDMI-A";
	case DRM_MODE_CONNECTOR_HDMIB:		return "HDMI-B";
	case DRM_MODE_CONNECTOR_DisplayPort:	return "DP";
	case DRM_MODE_CONNECTOR_eDP:		return "eDP";
	case DRM_MODE_CONNECTOR_LVDS:		return "LVDS";
	case DRM_MODE_CONNECTOR_VGA:		return "VGA";
	case DRM_MODE_CONNECTOR_DVII:		return "DVI-I";
	case DRM_MODE_CONNECTOR_DVID:		return "DVI-D";
	case DRM_MODE_CONNECTOR_DVIA:		return "DVI-A";
	case DRM_MODE_CONNECTOR_VIRTUAL:	return "Virtual";
	case DRM_MODE_CONNECTOR_Composite:	return "Composite";
	case DRM_MODE_CONNECTOR_TV:		return "TV";
	case DRM_MODE_CONNECTOR_WRITEBACK:	return "Writeback";
	default:				return "Unknown";
	}
}

static int pick_outputs(void)
{
	uint32_t taken[KKMS_MAX_OUT];
	int ntaken = 0;

	K.nout = 0;
	for (int i = 0; i < K.res->count_connectors &&
			K.nout < KKMS_MAX_OUT; i++) {
		drmModeConnector *c =
			drmModeGetConnector(K.drm_fd, K.res->connectors[i]);

		if (!c)
			continue;
		if (c->connection != DRM_MODE_CONNECTED || !c->count_modes) {
			drmModeFreeConnector(c);
			continue;
		}

		/* The PREFERRED mode, which is the panel's native one on a
		 * laptop; the first mode is only a fallback for a connector
		 * that flags none. */
		drmModeModeInfo *chosen = &c->modes[0];

		for (int m = 0; m < c->count_modes; m++)
			if (c->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
				chosen = &c->modes[m];
				break;
			}

		/* A CRTC that can drive this connector. The encoder it is
		 * already using is tried first, because taking it needs no
		 * reasoning about what else is lit. */
		uint32_t crtc = 0;

		if (c->encoder_id) {
			drmModeEncoder *e =
				drmModeGetEncoder(K.drm_fd, c->encoder_id);

			if (e) {
				crtc = e->crtc_id;
				drmModeFreeEncoder(e);
			}
		}

		for (int e = 0; !crtc && e < c->count_encoders; e++) {
			drmModeEncoder *enc =
				drmModeGetEncoder(K.drm_fd, c->encoders[e]);

			if (!enc)
				continue;
			for (int k = 0; k < K.res->count_crtcs; k++)
				if (enc->possible_crtcs & (1u << k)) {
					crtc = K.res->crtcs[k];
					break;
				}
			drmModeFreeEncoder(enc);
		}

		for (int t = 0; crtc && t < ntaken; t++)
			if (taken[t] == crtc)
				crtc = 0;

		if (!crtc) {
			drmModeFreeConnector(c);
			continue;
		}

		struct kkms_out *o = &K.out[K.nout++];

		taken[ntaken++] = crtc;
		o->connector = c->connector_id;
		o->crtc = crtc;
		o->mode = *chosen;
		o->width = chosen->hdisplay;
		o->height = chosen->vdisplay;

		/*
		 * THE LIST, TAKEN WHILE THE CONNECTOR IS OPEN. Re-opening one
		 * later asks the kernel to probe the monitor again, which is a
		 * modeset-shaped stall a picker must not pay every time
		 * somebody looks at it.
		 */
		o->nmodes = c->count_modes < KKMS_MAX_MODES
			    ? (int)c->count_modes : KKMS_MAX_MODES;
		o->cur_mode = 0;
		for (int m = 0; m < o->nmodes; m++) {
			o->modes[m] = c->modes[m];
			if (&c->modes[m] == chosen)
				o->cur_mode = m;
		}

		/* THE NAME A PERSON READS. libdrm has no connector-name call;
		 * the type string and the type id are what every tool builds
		 * one from, and they are what a monitor is labelled with in
		 * every other desktop. */
		snprintf(o->name, sizeof(o->name), "%s-%u",
			 conn_type_name(c->connector_type), c->connector_type_id);
		drmModeFreeConnector(c);
	}

	if (!K.nout)
		return fail_with("pick_outputs",
				 "no connector is connected with a mode and a CRTC that can drive it");
	return 0;
}

/*
 * THE DESKTOP'S SHAPE, FROM THE OUTPUTS' MODES.
 *
 * The grid is DERIVED and never stored, here as everywhere else: the virtual
 * box is the outputs laid end to end, and the number of cells is that box
 * divided by the cell. Recomputed whenever the font changes or a screen is
 * plugged in, because both change the answer.
 */
static void lay_out(void)
{
	int cw = kcell_w(), ch = kcell_h();
	int col = 0, rows = 0;

	if (cw < 1)
		cw = 1;
	if (ch < 1)
		ch = 1;

	K.vw = K.vh = 0;
	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		o->cols = o->width / cw;
		o->rows = o->height / ch;
		if (o->cols < 1)
			o->cols = 1;
		if (o->rows < 1)
			o->rows = 1;
		o->col = col;
		col += o->cols;
		if (o->rows > rows)
			rows = o->rows;
		K.vw += o->width;
		if (o->height > K.vh)
			K.vh = o->height;
	}

	/*
	 * THE SHARED GRID IS THE TALLEST SCREEN'S. A shorter one shows the
	 * top of it and is PADDED at the bottom rather than scaled — every
	 * cell is the same size on every screen, which is the whole reason a
	 * window dragged across the seam keeps its shape.
	 *
	 * The slice is CENTRED horizontally in whatever pixels are left over
	 * when the mode is not a whole number of cells wide; a grid pinned to
	 * the left edge leaves a bright strip down the right of every screen.
	 */
	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		o->px = (o->width - o->cols * cw) / 2;
		o->py = (o->height - o->rows * ch) / 2;
	}
	(void)rows;
}

/*
 * A DUMB BUFFER, and one of them.
 *
 * Not a page-flipped pair: this is a character grid, kcell_paint repaints only
 * the rows that changed, and a second buffer would mean every changed row is
 * copied twice and the diff has to be kept per buffer. What a flip would buy
 * is tear-free scrolling of full-screen video, which is not what this backend
 * is for. Say so here rather than let somebody assume it was an oversight.
 */
static int make_fb(struct kkms_out *o)
{
	struct drm_mode_create_dumb create = { 0 };
	struct drm_mode_map_dumb map = { 0 };

	create.width = o->width;
	create.height = o->height;
	create.bpp = 32;

	if (drmIoctl(K.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0)
		return fail_with("create dumb buffer", strerror(errno));

	o->handle = create.handle;
	o->stride = create.pitch;
	o->size = create.size;

	if (drmModeAddFB(K.drm_fd, o->width, o->height, 24, 32, o->stride,
			 o->handle, &o->fb) != 0)
		return fail_with("drmModeAddFB", strerror(errno));

	map.handle = o->handle;
	if (drmIoctl(K.drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0)
		return fail_with("map dumb buffer", strerror(errno));

	o->pixels = mmap(NULL, o->size, PROT_READ | PROT_WRITE, MAP_SHARED,
			 K.drm_fd, (off_t)map.offset);
	if (o->pixels == MAP_FAILED) {
		o->pixels = NULL;
		return fail_with("mmap the framebuffer", strerror(errno));
	}

	memset(o->pixels, 0, o->size);

	o->image = pixman_image_create_bits(PIXMAN_x8r8g8b8, o->width,
					    o->height, o->pixels,
					    (int)o->stride);
	if (!o->image)
		return fail_with("pixman_image_create_bits", "out of memory");

	return 0;
}

/*
 * THIS OUTPUT'S SLICE OF THE SHARED GRID, and the frame it last painted.
 *
 * Allocated per output and never shared: `kcell_paint` decides which rows to
 * repaint by comparing `cur` against `prev`, so two screens sharing one `prev`
 * would each find the other's paint already done and neither would redraw.
 */
static int make_slice(struct kkms_out *o)
{
	int n = o->cols * o->rows;

	if (n <= 0)
		return fail_with("the output's grid", "no cells");
	if (o->ncell != n) {
		free(o->cur);
		free(o->prev);
		o->cur = calloc((size_t)n, sizeof(*o->cur));
		o->prev = calloc((size_t)n, sizeof(*o->prev));
		o->ncell = o->cur && o->prev ? n : 0;
		if (!o->ncell)
			return fail_with("the output's grid", "out of memory");
	}
	o->force_full = 1;
	return 0;
}

/*
 * ── a screen plugged in after login ──────────────────────────────────────
 *
 * A MONITOR OF OUR OWN, because there was none. libinput's udev context
 * watches the `input` subsystem and nothing else, so nothing in this library
 * ever heard about `drm` — and `K.res` is a snapshot taken once at open, so a
 * re-probe against it would see the connector set as it was at startup however
 * often it ran.
 *
 * THE EVENT IS A HINT AND NOTHING MORE. `HOTPLUG=1` on a `drm` device says
 * something about the outputs changed; which connector, and whether it went or
 * arrived, is what a fresh probe answers. So the handler throws the resources
 * away and asks again, which is also what makes an unplug work — a connector
 * that is no longer connected simply does not come back in the list.
 */
static void hotplug_init(void)
{
	K.hotplug_fd = -1;
	K.hotplug_udev = udev_new();
	if (!K.hotplug_udev)
		return;
	K.hotplug = udev_monitor_new_from_netlink(K.hotplug_udev, "udev");
	if (!K.hotplug) {
		udev_unref(K.hotplug_udev);
		K.hotplug_udev = NULL;
		return;
	}
	udev_monitor_filter_add_match_subsystem_devtype(K.hotplug, "drm",
							NULL);
	if (udev_monitor_enable_receiving(K.hotplug) < 0) {
		udev_monitor_unref(K.hotplug);
		K.hotplug = NULL;
		udev_unref(K.hotplug_udev);
		K.hotplug_udev = NULL;
		return;
	}
	K.hotplug_fd = udev_monitor_get_fd(K.hotplug);
}

/* Everything an output owns, given back. Called per output on a re-probe and
 * by the shutdown, so the teardown is written once. */
static void out_free(struct kkms_out *o)
{
	if (o->image) {
		pixman_image_unref(o->image);
		o->image = NULL;
	}
	if (o->pixels) {
		munmap(o->pixels, o->size);
		o->pixels = NULL;
	}
	if (o->fb) {
		drmModeRmFB(K.drm_fd, o->fb);
		o->fb = 0;
	}
	if (o->handle) {
		struct drm_mode_destroy_dumb d = { .handle = o->handle };

		drmIoctl(K.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
		o->handle = 0;
	}
	free(o->cur);
	free(o->prev);
	o->cur = o->prev = NULL;
	o->ncell = 0;
}

/*
 * LAY THE SCREENS OUT AND LIGHT THEM, from whatever the modes now say.
 *
 * The one place buffers are made and CRTCs are programmed, so a hotplug and a
 * mode change take the same path — two of them would be two answers to what a
 * screen is, and the second one would be wrong the first time a field was
 * added. Returns 0, or -1 having left as many outputs lit as it managed.
 */
static int relight(void)
{
	lay_out();
	for (int i = 0; i < K.nout; i++) {
		if (make_fb(&K.out[i]) != 0 || make_slice(&K.out[i]) != 0) {
			K.nout = i;
			return -1;
		}
		if (drmModeSetCrtc(K.drm_fd, K.out[i].crtc, K.out[i].fb, 0, 0,
				   &K.out[i].connector, 1,
				   &K.out[i].mode) != 0)
			return -1;
	}
	return 0;
}

/*
 * THE OUTPUTS, AGAIN, AFTER SOMETHING CHANGED.
 *
 * Returns 1 when the grid moved, which is what the caller announces — a screen
 * plugged in is the same event as a screen resized, and the session already
 * knows how to handle that. Returns 0 when nothing came of it, including the
 * case where the probe failed: a machine that has just lost its only monitor
 * keeps the buffers it has rather than tearing the desktop down, because the
 * monitor may come back and the session is still running either way.
 */
static int reprobe(void)
{
	int ow = K.vw, oh = K.vh, on = K.nout;

	for (int i = 0; i < K.nout; i++)
		out_free(&K.out[i]);

	if (K.res)
		drmModeFreeResources(K.res);
	K.res = drmModeGetResources(K.drm_fd);
	if (!K.res || pick_outputs() != 0) {
		K.nout = 0;
		return 0;
	}

	if (relight() != 0)
		return 0;
	return K.nout != on || K.vw != ow || K.vh != oh;
}

int kkms_outputs(void)
{
	return K.nout;
}

int kkms_output(int i, KkmsOutput *out)
{
	if (i < 0 || i >= K.nout || !out)
		return 0;
	out->width = K.out[i].width;
	out->height = K.out[i].height;
	out->col = K.out[i].col;
	out->cols = K.out[i].cols;
	out->rows = K.out[i].rows;
	out->connector = K.out[i].connector;
	snprintf(out->name, sizeof(out->name), "%s", K.out[i].name);
	out->nmodes = K.out[i].nmodes;
	out->cur_mode = K.out[i].cur_mode;
	return 1;
}

int kkms_modes(int out)
{
	return out >= 0 && out < K.nout ? K.out[out].nmodes : 0;
}

int kkms_mode(int out, int i, KkmsMode *m)
{
	if (out < 0 || out >= K.nout || !m)
		return 0;
	if (i < 0 || i >= K.out[out].nmodes)
		return 0;

	const drmModeModeInfo *d = &K.out[out].modes[i];

	m->width = d->hdisplay;
	m->height = d->vdisplay;
	/*
	 * MILLIHERTZ, AND COMPUTED RATHER THAN READ. `vrefresh` is a rounded
	 * integer the kernel fills in for convenience; the clock and the
	 * totals are the mode, and 59.94 Hz reported as 59 is two modes a
	 * picker cannot tell apart.
	 */
	m->refresh = d->htotal && d->vtotal
		     ? (int)(((uint64_t)d->clock * 1000000ull) /
			     ((uint64_t)d->htotal * d->vtotal))
		     : (int)(d->vrefresh * 1000);
	m->preferred = (d->type & DRM_MODE_TYPE_PREFERRED) != 0;
	return 1;
}

int kkms_mode_current(int out)
{
	return out >= 0 && out < K.nout ? K.out[out].cur_mode : -1;
}

int kkms_set_mode(int out, int i)
{
	if (out < 0 || out >= K.nout || i < 0 || i >= K.out[out].nmodes)
		return -1;

	struct kkms_out *o = &K.out[out];
	drmModeModeInfo was = o->mode;
	int was_i = o->cur_mode;

	o->mode = o->modes[i];
	o->width = o->mode.hdisplay;
	o->height = o->mode.vdisplay;
	o->cur_mode = i;

	/*
	 * EVERY OUTPUT, not this one. Screens are laid edge to edge, so a
	 * wider mode moves every column after it and every slice behind it is
	 * the wrong width — re-cutting one would leave the rest painting into
	 * buffers that no longer match the grid they are given.
	 */
	if (relight() == 0)
		return 0;

	/* THE OLD MODE COMES BACK. A screen is the one thing a person cannot
	 * work around from somewhere else, so a mode the driver refused must
	 * not leave the desktop on a screen nobody can read. */
	o->mode = was;
	o->width = was.hdisplay;
	o->height = was.vdisplay;
	o->cur_mode = was_i;
	relight();
	return -1;
}

int kkms_hotplug_fd(void)
{
	return K.hotplug_fd;
}

int kkms_hotplug_pump(void)
{
	struct udev_device *d;
	int changed = 0;

	if (!K.hotplug)
		return 0;
	/* DRAINED, not read once: several events arrive for one plug and a
	 * descriptor left readable spins the caller's poll. */
	while ((d = udev_monitor_receive_device(K.hotplug))) {
		const char *hot = udev_device_get_property_value(d, "HOTPLUG");

		if (hot && *hot == '1')
			changed = 1;
		udev_device_unref(d);
	}
	return changed && K.active ? reprobe() : 0;
}

/* ── the backend ─────────────────────────────────────────────────────── */

/*
 * THE SCANOUT BUFFER IS WRITTEN BY THE CPU AND THE DRIVER HAS TO BE TOLD.
 *
 * This is a dumb buffer: painting goes into a mapping of guest memory, and a
 * card that scans that memory out directly needs nothing more. A VIRTUAL one
 * does — virtio-gpu, qxl, bochs and vmwgfx all keep the displayed image on the
 * host side and copy it up when the guest marks the framebuffer dirty.
 *
 * A driver with no dirty hook answers EINVAL, which is not a failure here —
 * it is a card that already showed the pixels.
 */
static void kkms_dirty(const struct kkms_out *o)
{
	drmModeClip clip = {
		.x1 = 0, .y1 = 0,
		.x2 = (unsigned short)o->width,
		.y2 = (unsigned short)o->height,
	};

	drmModeDirtyFB(K.drm_fd, o->fb, &clip, 1);
}

/*
 * ONE FRAME, CUT INTO AS MANY SCREENS AS THERE ARE.
 *
 * The session composes ONE grid and knows nothing about screens — it is told
 * how many cells there are and that is all — so the cut is here, where the
 * modes are. Each output copies its own columns out of the shared frame into
 * its own slice and paints that: the copy is what lets each keep its own row
 * diff, which is what makes a clock ticking on one screen not repaint the
 * other.
 *
 * A SCREEN SHORTER THAN THE GRID SHOWS THE TOP OF IT. The rows past its own
 * are not drawn and the pixels below the slice stay black — padded, never
 * scaled, because a character grid stretched to fit is a grid whose cells are
 * the wrong shape and nothing downstream would notice.
 */
static void kkms_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		       int force_full)
{
	(void)prev;
	if (!K.active)
		return;

	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];
		int full = force_full || o->force_full;

		if (!o->image || !o->cur || !o->prev)
			continue;

		/* THE COLUMNS THIS SCREEN SHOWS, row by row. A row past the
		 * shared frame's height leaves the slice's own row as it was,
		 * which is the black it was allocated as. */
		for (int y = 0; y < o->rows; y++) {
			KtuiCell *dst = o->cur + (size_t)y * o->cols;

			if (y >= h) {
				memset(dst, 0,
				       (size_t)o->cols * sizeof(*dst));
				continue;
			}
			for (int x = 0; x < o->cols; x++) {
				int sx = o->col + x;

				if (sx < w)
					dst[x] = cur[(size_t)y * w + sx];
				else
					memset(&dst[x], 0, sizeof(dst[x]));
			}
		}

		/*
		 * Asked BEFORE the paint, because the paint is what makes prev
		 * equal to cur. A flush is called for every turn of the view's
		 * loop and most of them change nothing; marking the whole
		 * screen dirty regardless would hand the host a full frame
		 * fifty times a second for a desktop that redraws when a clock
		 * ticks.
		 */
		int changed = full ||
			      memcmp(o->cur, o->prev,
				     (size_t)o->ncell * sizeof(*o->cur));

		o->force_full = 0;
		kcell_paint(o->image, o->cur, o->prev, o->cols, o->rows, full,
			    1, o->width, o->height);
		if (changed)
			kkms_dirty(o);
	}
}

static void kkms_size(int *w, int *h)
{
	int cw = kcell_w(), ch = kcell_h();

	*w = cw > 0 ? K.vw / cw : 80;
	*h = ch > 0 ? K.vh / ch : 24;
	if (*w < 1)
		*w = 1;
	if (*h < 1)
		*h = 1;
}

static int kkms_caps(void)
{
	/* Our own renderer, so "does the terminal support it" has no meaning.
	 * Not LINUXVT: that flag is about the kernel's console, and this
	 * replaces it rather than running on it. */
	return KT_CAP_TRUECOLOR | KT_CAP_UTF8 | KT_CAP_MOUSE;
}

static const KtuiBackend kkms_backend = {
	.name = "kms",
	.flush = kkms_flush,
	.poll_event = kkms_poll_event,
	.size = kkms_size,
	.caps = kkms_caps,
};

int kkms_active(void)
{
	return K.active;
}

int kkms_seat_fd(void)
{
	return K.seat ? libseat_get_fd(K.seat) : -1;
}

void kkms_pump(void)
{
	if (K.seat)
		libseat_dispatch(K.seat, 0);
	kkms_input_pump();
}

int kkms_init(const char *seat_name, const char *card, const char *font)
{
	memset(&K, 0, sizeof(K));
	K.drm_fd = -1;
	K.drm_dev = -1;
	reason[0] = '\0';

	(void)seat_name;	/* libseat takes the seat from the environment */

	K.seat = libseat_open_seat(&seat_listener, NULL);
	if (!K.seat) {
		/* No shutdown: nothing was opened, and libseat_close_seat on a
		 * null seat is not a call this makes. */
		return fail_with("libseat_open_seat",
				 "no seat daemon and no direct-session privilege");
	}

	/* The first dispatch is what delivers the initial enable, and nothing
	 * can be opened before the session is active. */
	if (libseat_dispatch(K.seat, -1) < 0) {
		fail_with("libseat_dispatch",
			  "the seat never delivered its initial enable");
		goto fail;
	}

	if (open_drm(card && *card ? card : NULL) != 0)
		goto fail;
	if (pick_outputs() != 0)
		goto fail;
	if (kcell_font_load(font) != 0) {
		fail_with("kcell_font_load", font ? font : "the default console font");
		goto fail;
	}
	snprintf(K.font, sizeof(K.font), "%s", font ? font : "");
	lay_out();
	for (int i = 0; i < K.nout; i++) {
		if (make_fb(&K.out[i]) != 0)
			goto fail;
		if (make_slice(&K.out[i]) != 0)
			goto fail;
	}

	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		if (drmModeSetCrtc(K.drm_fd, o->crtc, o->fb, 0, 0,
				   &o->connector, 1, &o->mode) != 0) {
			fail_with("drmModeSetCrtc", strerror(errno));
			goto fail;
		}
	}

	if (kkms_input_init() != 0) {
		fail_with("kkms_input_init",
			  "libinput opened none of the seat's devices");
		goto fail;
	}

	hotplug_init();
	ktui_backend_set(&kkms_backend);

	/*
	 * ONE LINE, ON SUCCESS, NAMING WHAT WAS TAKEN. "no screen to take"
	 * says a screen was not taken and nothing says which one was, so a
	 * display that comes up wrong — the wrong card, a mode nobody wanted,
	 * a seat that never went active and therefore never draws — is a black
	 * rectangle with no way to tell those apart.
	 */
	int gw = 0, gh = 0;

	kkms_size(&gw, &gh);
	fprintf(stderr,
		"kdos-view: mode kms — %d output(s), %dx%u virtual, seat %s, "
		"cell %dx%d, grid %dx%d\n",
		K.nout, K.vw, (unsigned)K.vh,
		K.active ? "active" : "INACTIVE",
		kcell_w(), kcell_h(), gw, gh);
	for (int i = 0; i < K.nout; i++) {
		const struct kkms_out *o = &K.out[i];

		fprintf(stderr,
			"kdos-view:   output %d: %ux%u, crtc %u, "
			"connector %u, columns %d..%d\n",
			i, o->mode.hdisplay, o->mode.vdisplay, o->crtc,
			o->connector, o->col, o->col + o->cols - 1);
	}
	return 0;

fail:
	kkms_shutdown();
	return -1;
}

/*
 * A DIFFERENT FONT ON THE SAME SCREEN.
 *
 * The grid is derived and not stored — kkms_size() divides the mode by the
 * cell — so a font with a different cell is a different number of columns and
 * rows, and everything that reads ktui_w/ktui_h has to be told. This reloads
 * and reports; the CALLER calls ktui_draw_resize() and announces the grid,
 * because only the caller knows who is listening.
 *
 * THE OLD FONT COMES BACK IF THE NEW ONE WILL NOT LOAD. A screen is the one
 * thing a person cannot work around from somewhere else.
 */
int kkms_set_font(const char *font)
{
	char prev[sizeof(K.font)];

	if (!K.nout)
		return -1;
	snprintf(prev, sizeof(prev), "%s", K.font);

	kcell_font_free();
	if (kcell_font_load(font && *font ? font : NULL) != 0) {
		if (kcell_font_load(prev[0] ? prev : NULL) != 0)
			return -1;	/* nothing draws now; the caller exits */
		lay_out();
		for (int i = 0; i < K.nout; i++)
			make_slice(&K.out[i]);
		return -1;
	}
	snprintf(K.font, sizeof(K.font), "%s", font ? font : "");
	/* A DIFFERENT CELL IS A DIFFERENT NUMBER OF CELLS PER SCREEN, so the
	 * layout and every slice are recut before anything draws again. */
	lay_out();
	for (int i = 0; i < K.nout; i++)
		if (make_slice(&K.out[i]) != 0)
			return -1;
	return 0;
}

const char *kkms_font(void)
{
	return K.font;
}

void kkms_shutdown(void)
{
	kkms_input_shutdown();

	for (int i = 0; i < K.nout; i++)
		out_free(&K.out[i]);
	K.nout = 0;
	if (K.hotplug) {
		udev_monitor_unref(K.hotplug);
		K.hotplug = NULL;
		K.hotplug_fd = -1;
	}
	if (K.hotplug_udev) {
		udev_unref(K.hotplug_udev);
		K.hotplug_udev = NULL;
	}
	if (K.res) {
		drmModeFreeResources(K.res);
		K.res = NULL;
	}
	if (K.seat && K.drm_dev >= 0) {
		libseat_close_device(K.seat, K.drm_dev);
		K.drm_dev = -1;
		K.drm_fd = -1;
	}
	if (K.seat) {
		libseat_close_seat(K.seat);
		K.seat = NULL;
	}

	kcell_font_free();
	ktui_backend_set(NULL);
}

/*
 * Power the screen down and back up.
 *
 * drmModeSetCrtc with no framebuffer, not a DPMS property write: DPMS is a
 * connector property that legacy and atomic drivers expose differently and
 * some virtual drivers do not expose at all, whereas detaching the CRTC is the
 * one operation every KMS driver implements. The mode is set again on the way
 * back, because a CRTC that was turned off has no mode to return to.
 *
 * Does nothing while the session is switched away: the device is not ours to
 * program then, and the VT we switched to has already taken the screen.
 */
int kkms_switch_vt(int n)
{
	if (!K.seat || n < 1 || n > 63)
		return -1;
	return libseat_switch_session(K.seat, n) == 0 ? 0 : -1;
}

void kkms_blank(int on)
{
	if (K.drm_fd < 0 || !K.nout || !kkms_active())
		return;

	/* EVERY screen: one left lit while the rest went dark would be a
	 * machine that looks half asleep. */
	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		if (on)
			drmModeSetCrtc(K.drm_fd, o->crtc, 0, 0, 0, NULL, 0,
				       NULL);
		else
			drmModeSetCrtc(K.drm_fd, o->crtc, o->fb, 0, 0,
				       &o->connector, 1, &o->mode);
	}
}
