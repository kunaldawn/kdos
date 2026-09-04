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
	if (K.drm_fd >= 0 && K.crtc && K.fb)
		drmModeSetCrtc(K.drm_fd, K.crtc, K.fb, 0, 0, &K.connector, 1,
			       &K.mode);
	K.force_full = 1;
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

static int open_drm(void)
{
	/*
	 * The first card with a connected output wins. Enumerated rather than
	 * assumed: card0 is whichever device the kernel probed first, which on
	 * a machine with a discrete card and an integrated one is not
	 * necessarily the one with a screen on it.
	 */
	int opened = 0;

	for (int i = 0; i < 8; i++) {
		char path[64];

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

	return fail_with("open_drm",
			 opened ? "a DRM device opened and reports no connectors"
				: "the seat opened no /dev/dri/card0..7");
}

static int pick_mode(void)
{
	for (int i = 0; i < K.res->count_connectors; i++) {
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

		if (!crtc) {
			drmModeFreeConnector(c);
			continue;
		}

		K.connector = c->connector_id;
		K.crtc = crtc;
		K.mode = *chosen;
		K.width = chosen->hdisplay;
		K.height = chosen->vdisplay;
		drmModeFreeConnector(c);
		return 0;
	}

	return fail_with("pick_mode",
			 "no connector is connected with a mode and a CRTC that can drive it");
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
static int make_fb(void)
{
	struct drm_mode_create_dumb create = { 0 };
	struct drm_mode_map_dumb map = { 0 };

	create.width = K.width;
	create.height = K.height;
	create.bpp = 32;

	if (drmIoctl(K.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0)
		return fail_with("create dumb buffer", strerror(errno));

	K.handle = create.handle;
	K.stride = create.pitch;
	K.size = create.size;

	if (drmModeAddFB(K.drm_fd, K.width, K.height, 24, 32, K.stride,
			 K.handle, &K.fb) != 0)
		return fail_with("drmModeAddFB", strerror(errno));

	map.handle = K.handle;
	if (drmIoctl(K.drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0)
		return fail_with("map dumb buffer", strerror(errno));

	K.pixels = mmap(NULL, K.size, PROT_READ | PROT_WRITE, MAP_SHARED,
			K.drm_fd, (off_t)map.offset);
	if (K.pixels == MAP_FAILED) {
		K.pixels = NULL;
		return fail_with("mmap the framebuffer", strerror(errno));
	}

	memset(K.pixels, 0, K.size);

	K.image = pixman_image_create_bits(PIXMAN_x8r8g8b8, K.width, K.height,
					   K.pixels, (int)K.stride);
	if (!K.image)
		return fail_with("pixman_image_create_bits", "out of memory");

	return 0;
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
static void kkms_dirty(void)
{
	drmModeClip clip = {
		.x1 = 0, .y1 = 0,
		.x2 = (unsigned short)K.width,
		.y2 = (unsigned short)K.height,
	};

	drmModeDirtyFB(K.drm_fd, K.fb, &clip, 1);
}

static void kkms_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		       int force_full)
{
	if (!K.active || !K.image)
		return;

	int full = force_full || K.force_full;
	/*
	 * Asked BEFORE the paint, because the paint is what makes prev equal
	 * to cur. A flush is called for every turn of the view's loop and most
	 * of them change nothing; marking the whole screen dirty regardless
	 * would hand the host a full frame fifty times a second for a desktop
	 * that redraws when a clock ticks.
	 */
	int changed = full || !prev ||
		      memcmp(cur, prev, (size_t)w * (size_t)h * sizeof(*cur));

	K.force_full = 0;
	kcell_paint(K.image, cur, prev, w, h, full, 1, K.width, K.height);
	if (changed)
		kkms_dirty();
}

static void kkms_size(int *w, int *h)
{
	int cw = kcell_w(), ch = kcell_h();

	*w = cw > 0 ? K.width / cw : 80;
	*h = ch > 0 ? K.height / ch : 24;
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

int kkms_init(const char *seat_name, const char *font)
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

	if (open_drm() != 0)
		goto fail;
	if (pick_mode() != 0)
		goto fail;
	if (kcell_font_load(font) != 0) {
		fail_with("kcell_font_load", font ? font : "the default console font");
		goto fail;
	}
	if (make_fb() != 0)
		goto fail;

	if (drmModeSetCrtc(K.drm_fd, K.crtc, K.fb, 0, 0, &K.connector, 1,
			   &K.mode) != 0) {
		fail_with("drmModeSetCrtc", strerror(errno));
		goto fail;
	}

	if (kkms_input_init() != 0) {
		fail_with("kkms_input_init",
			  "libinput opened none of the seat's devices");
		goto fail;
	}

	K.force_full = 1;
	ktui_backend_set(&kkms_backend);

	/*
	 * ONE LINE, ON SUCCESS, NAMING WHAT WAS TAKEN. "no screen to take"
	 * says a screen was not taken and nothing says which one was, so a
	 * display that comes up wrong — the wrong card, a mode nobody wanted,
	 * a seat that never went active and therefore never draws — is a black
	 * rectangle with no way to tell those apart.
	 */
	fprintf(stderr,
		"kdos-view: kms %ux%u, crtc %u, connector %u, seat %s, "
		"cell %dx%d, grid %dx%d\n",
		K.width, K.height, K.crtc, K.connector,
		K.active ? "active" : "INACTIVE",
		kcell_w(), kcell_h(),
		kcell_w() > 0 ? (int)(K.width / (unsigned)kcell_w()) : 0,
		kcell_h() > 0 ? (int)(K.height / (unsigned)kcell_h()) : 0);
	return 0;

fail:
	kkms_shutdown();
	return -1;
}

void kkms_shutdown(void)
{
	kkms_input_shutdown();

	if (K.image) {
		pixman_image_unref(K.image);
		K.image = NULL;
	}
	if (K.pixels) {
		munmap(K.pixels, K.size);
		K.pixels = NULL;
	}
	if (K.fb) {
		drmModeRmFB(K.drm_fd, K.fb);
		K.fb = 0;
	}
	if (K.handle) {
		struct drm_mode_destroy_dumb d = { .handle = K.handle };

		drmIoctl(K.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
		K.handle = 0;
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
	if (K.drm_fd < 0 || !K.crtc || !kkms_active())
		return;

	if (on) {
		drmModeSetCrtc(K.drm_fd, K.crtc, 0, 0, 0, NULL, 0, NULL);
		return;
	}

	drmModeSetCrtc(K.drm_fd, K.crtc, K.fb, 0, 0, &K.connector, 1, &K.mode);
}
