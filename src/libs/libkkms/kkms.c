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
#include <poll.h>

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

/*
 * THE SEAT NAME, KEPT OUTSIDE `K` FOR THE SAME REASON. It is read by
 * kkms_input_init(), which runs after the struct has been cleared and filled,
 * and it is put into the environment before libseat opens the seat so that
 * libseat and libinput cannot end up on different ones.
 */
static char seat[64];

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

/* The buffer the CRTC is showing. */
static uint32_t front_fb(const struct kkms_out *o);
/* Every row of this output is a frame behind in every buffer it holds. */
static void owe_all(struct kkms_out *o);
/* Nothing is in flight and nothing is waiting: every buffer but the front one
 * is the painter's again. Used wherever a flip is ABANDONED rather than
 * completed — a VT switch, a modeset, a screen going dark. */
static void flip_reset(struct kkms_out *o);

static void on_enable(struct libseat *seat, void *data)
{
	(void)seat;
	(void)data;
	K.active = 1;

	/*
	 * THE INPUT DEVICES ARE ASKED FOR AGAIN. The seat revokes every evdev
	 * descriptor it handed out when it deactivates a session and does not
	 * hand them back by itself; a client that did not ask returns with a
	 * keyboard and a pointer that never report anything again.
	 */
	if (K.li)
		libinput_resume(K.li);

	/* The mode has to be set again: the VT we came back to was somebody
	 * else's and its CRTC is theirs. */
	/* EVERY screen, not the primary alone: the VT we came back to had all
	 * of them and left every CRTC in an unknown state. */
	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		/* A SCREEN THAT WENT AWAY BLANKED COMES BACK BLANKED. The
		 * VT we return from left every CRTC lit; re-attaching the
		 * framebuffer here while K.blanked is set would light a
		 * screen the session believes is asleep and then paint
		 * nothing on it, because kkms_flush() returns on the flag.
		 * kkms_blank(0) is the one path that relights, and it owes
		 * every output a whole frame. */
		if (K.drm_fd >= 0 && o->crtc && K.blanked)
			drmModeSetCrtc(K.drm_fd, o->crtc, 0, 0, 0, NULL, 0,
				       NULL);
		else if (K.drm_fd >= 0 && o->crtc && front_fb(o))
			drmModeSetCrtc(K.drm_fd, o->crtc, front_fb(o), 0, 0,
				       &o->connector, 1, &o->mode);
		/*
		 * A FLIP THE KERNEL WILL NEVER REPORT, and a frame composed
		 * for a screen that is now somebody else's: the device was
		 * not ours while we were away. The generation moves inside
		 * flip_reset(), or a completion that does arrive names the
		 * buffer it flipped as the front one and the next paint goes
		 * into the buffer the raster is inside.
		 */
		flip_reset(o);
		o->force_full = 1;
		/* And the toolkit is told, because it is what decides whether
		 * a flush happens at all: a screen coming back has nothing
		 * new drawn on it and everything to repaint. */
		ktui_draw_invalidate();
	}
}

static void on_disable(struct libseat *seat, void *data)
{
	(void)data;
	K.active = 0;
	/* The descriptors go before the seat takes them, which is what lets
	 * libinput ask for fresh ones on the way back. */
	if (K.li)
		libinput_suspend(K.li);
	/* Acknowledged immediately: the switch does not complete until it is,
	 * and a session that sits on it wedges the machine's VT switching. */
	libseat_disable_seat(seat);
}

static struct libseat_seat_listener seat_listener = {
	.enable_seat = on_enable,
	.disable_seat = on_disable,
};

/* ── the device ──────────────────────────────────────────────────────── */

/*
 * DOES THIS DRIVER SHOW THE BUFFER, OR COPY IT TO A HOST?
 *
 * virtio_gpu, qxl and vmwgfx keep the displayed image on the host side and
 * read guest memory only at the transfer drmModeDirtyFB asks for. Two things
 * follow, and both are the opposite of what a real scanout wants: painting in
 * place cannot tear, because nothing reads the buffer between transfers; and
 * a legacy page flip carries no damage rectangle, so the driver uploads the
 * WHOLE plane — 8 MB a frame at 1080p to deliver the few kilobytes a clock
 * tick changed.
 *
 * The other virtual drivers are NOT in this list. QEMU's stdvga (bochs) and a
 * handed-over simpledrm framebuffer are scanned continuously, so a single
 * buffer there tears exactly like hardware and a flip is what removes it.
 */
static int driver_transfers(int fd)
{
	drmVersionPtr v = drmGetVersion(fd);
	int yes = 0;

	if (!v)
		return 0;
	/*
	 * EVERY DRIVER THAT MOVES THE PIXELS SOMEWHERE ELSE. For these the
	 * dirty rectangle is the presentation and its size is the bandwidth,
	 * so they take the single-buffer path where the rectangle is sent —
	 * a flip would present the whole plane and upload all of it, which
	 * over USB is the difference between a corner and eight megabytes.
	 */
	if (v->name)
		yes = strcmp(v->name, "virtio_gpu") == 0 ||
		      strcmp(v->name, "qxl") == 0 ||
		      strcmp(v->name, "vmwgfx") == 0 ||
		      strcmp(v->name, "udl") == 0 ||
		      strcmp(v->name, "gud") == 0 ||
		      strcmp(v->name, "hyperv_drm") == 0 ||
		      strcmp(v->name, "vboxvideo") == 0;
	drmFreeVersion(v);
	return yes;
}

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
		int lit = 0;

		/*
		 * A CONNECTOR IS NOT A SCREEN. Taking the first card that
		 * merely HAS connectors is how a machine with a discrete card
		 * and an integrated one takes the card nothing is plugged
		 * into: it opens, reports eight connectors, none connected,
		 * and the sweep stops there — which is the failure this loop
		 * exists to avoid and what its own comment promises it does.
		 */
		for (int c = 0; res && c < res->count_connectors && !lit; c++) {
			drmModeConnector *conn =
				drmModeGetConnector(fd, res->connectors[c]);

			if (!conn)
				continue;
			if (conn->connection == DRM_MODE_CONNECTED &&
			    conn->count_modes > 0)
				lit = 1;
			drmModeFreeConnector(conn);
		}

		if (lit) {
			uint64_t cap = 0;

			K.drm_fd = fd;
			K.drm_dev = id;
			K.res = res;
			K.drm_transfers = driver_transfers(fd);
			/*
			 * CAN THIS DEVICE PRESENT WITHOUT WAITING FOR THE
			 * VBLANK? DRM_CAP_ASYNC_PAGE_FLIP is exactly the
			 * legacy drmModePageFlip's answer, which is the call
			 * this library makes; the atomic capability is a
			 * different number and says nothing about it.
			 *
			 * ASKED ONCE, HERE, because the device cannot change
			 * — and because a flag passed to a driver that does
			 * not take it is refused with EINVAL, which is the
			 * one errno the flush reads as "this driver cannot
			 * flip" and answers by giving up every buffer but one.
			 */
			K.can_async = drmGetCap(fd, DRM_CAP_ASYNC_PAGE_FLIP,
						&cap) == 0 && cap;
			K.flip_flags = K.async_flip && K.can_async
				       ? DRM_MODE_PAGE_FLIP_ASYNC : 0;
			return 0;
		}

		if (res)
			drmModeFreeResources(res);
		libseat_close_device(K.seat, id);
	}

	if (card)
		return fail_with("open_drm",
				 opened ? "the named card has no connected screen"
					: "the seat would not open the named card");
	return fail_with("open_drm",
			 opened ? "a DRM device opened and has no connected screen"
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

/*
 * A MODE'S REFRESH IN MILLIHERTZ, FROM ITS TIMINGS.
 *
 * `vrefresh` is a rounded integer the kernel fills in for convenience, and
 * 59.94 Hz reported as 59 is two modes a picker cannot tell apart and a
 * session pacing itself to the wrong period. The clock and the totals ARE the
 * mode, so they are what this reads; `vrefresh` is the fallback for a mode
 * that publishes no totals at all.
 *
 * THE TIMINGS ARE NOT THE FRAME RATE ON THEIR OWN. Interlace sends two fields
 * per frame, doublescan sends each line twice and vscan sends each line n
 * times; the same three corrections the kernel applies in drm_mode_vrefresh,
 * in the same order, or 1080i is offered as 30 Hz beside the 60 Hz every other
 * tool reports.
 */
static int mode_refresh_mhz(const drmModeModeInfo *d)
{
	int r = d->htotal && d->vtotal
		? (int)(((uint64_t)d->clock * 1000000ull) /
			((uint64_t)d->htotal * d->vtotal))
		: (int)(d->vrefresh * 1000);

	if (d->flags & DRM_MODE_FLAG_INTERLACE)
		r *= 2;
	if (d->flags & DRM_MODE_FLAG_DBLSCAN)
		r /= 2;
	if (d->vscan > 1)
		r /= d->vscan;
	return r;
}

static int pick_outputs(void)
{
	uint32_t taken[KKMS_MAX_OUT];
	int ntaken = 0;
	/*
	 * WHAT EACH SCREEN WAS ALREADY SET TO, so a re-probe does not undo a
	 * choice a person made. A hotplug can arrive for any reason — another
	 * connector, a monitor waking — and every one of them re-ran this and
	 * went back to the preferred mode, so a chosen mode lasted until the
	 * next unrelated event.
	 */
	struct { uint32_t conn; drmModeModeInfo mode; int have; }
		was[KKMS_MAX_OUT];
	int nwas = K.nout;

	for (int i = 0; i < nwas && i < KKMS_MAX_OUT; i++) {
		was[i].conn = K.out[i].connector;
		was[i].mode = K.out[i].mode;
		was[i].have = 1;
	}

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

		/*
		 * AND, WHERE THE CALLER ASKED FOR IT, THE FASTEST MODE AT THE
		 * SIZE THE MONITOR CHOSE.
		 *
		 * THE RESOLUTION IS THE MONITOR'S AND ONLY THE REFRESH IS
		 * OURS: a panel's preferred mode is its native size, and a
		 * scaled one is a blurred desktop nobody asked for. So the
		 * search is bounded to modes of the same hdisplay and
		 * vdisplay, and a connector that publishes one refresh at its
		 * native size keeps exactly the mode it published.
		 *
		 * The refresh is computed from the timings, because `vrefresh`
		 * rounds 59.94 and 60 to the same integer and this comparison
		 * is the one place that difference decides which mode a
		 * screen wears.
		 */
		if (K.mode_policy == KKMS_MODE_FASTEST) {
			int best = mode_refresh_mhz(chosen);

			for (int m = 0; m < c->count_modes; m++) {
				int r;

				if (c->modes[m].hdisplay != chosen->hdisplay ||
				    c->modes[m].vdisplay != chosen->vdisplay)
					continue;
				r = mode_refresh_mhz(&c->modes[m]);
				if (r > best) {
					best = r;
					chosen = &c->modes[m];
				}
			}
		}

		/*
		 * Unless this screen was already set to one it still
		 * publishes, which is a decision and outranks the default.
		 *
		 * MATCHED ON THE COMPUTED REFRESH, for the reason the search
		 * above uses it: `vrefresh` rounds 59.94 and 60 to the same
		 * integer, and a connector that lists the 59.94 mode first
		 * would hand back that one every re-probe — a screen set to
		 * 60.00 dropping to 59.94 on any hotplug. The size is still
		 * part of the match, or a remembered mode is restored at a
		 * different resolution that happens to share its refresh.
		 */
		for (int k = 0; k < nwas; k++) {
			int was_r;

			if (!was[k].have || was[k].conn != c->connector_id)
				continue;
			was_r = mode_refresh_mhz(&was[k].mode);
			for (int m = 0; m < c->count_modes; m++)
				if (c->modes[m].hdisplay ==
					    was[k].mode.hdisplay &&
				    c->modes[m].vdisplay ==
					    was[k].mode.vdisplay &&
				    mode_refresh_mhz(&c->modes[m]) == was_r) {
					chosen = &c->modes[m];
					break;
				}
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
 *
 * THE BOX IS A WHOLE NUMBER OF CELLS, which is what makes the division exact.
 * Summing raw mode widths does not: floor(sum(w)/cw) can exceed sum(floor(w/cw))
 * whenever a mode is not a whole number of cells, and the columns that
 * difference invents belong to no screen's slice — the pointer reaches them,
 * nothing paints them, and a click in the last strip of the screen lands on
 * nothing. So each output contributes its own cell width and the trailing
 * partial cell of its mode is padding the pointer cannot enter.
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
		K.vw += o->cols * cw;
	}

	/*
	 * THE SHARED GRID IS THE TALLEST SCREEN'S. A shorter one shows the
	 * top of it and is PADDED at the bottom rather than scaled — every
	 * cell is the same size on every screen, which is the whole reason a
	 * window dragged across the seam keeps its shape.
	 *
	 * THE GRID STARTS AT THE TOP LEFT AND THE SLACK IS AT THE RIGHT AND
	 * THE BOTTOM of each screen, where its mode is not a whole number of
	 * cells. It is filled with KT_BG by kcell_paint()'s own padding, so it
	 * is the desktop's colour rather than a strip of whatever the
	 * framebuffer held — and it is outside the virtual box, so the
	 * pointer's cell is `pixel / cell` with no origin to subtract and
	 * never names a cell the paint does not cut.
	 */
	K.vh = rows * ch;
}

/*
 * A SCANOUT BUFFER. One of up to three, plus the painter's own in system
 * memory.
 *
 * A SECOND BUFFER is what makes a flip possible, and a flip is what makes an
 * animation tear-free: a single buffer is rewritten row by row while the
 * raster is inside it, and a row caught mid-paint shows the old glyph above
 * the new one. A THIRD is what stops the painter waiting for the vblank: with
 * two, the only buffer it may touch is the one a flip is waiting on, so a
 * compose that overruns a refresh period costs a whole further period. The
 * cost of each is a copy of the rows that changed, which on a cell grid is
 * what changed and nothing more.
 *
 * The painter never touches any of them. A dumb buffer is mapped
 * write-combined: reads from it go to memory at a few bytes a cycle, and
 * every glyph composite is a read-modify-write of its own rectangle. So the
 * cells are painted into `shadow_bits`, which is ordinary memory, and reach
 * the screen as a straight memcpy of whole rows — writes only, which is the
 * one access pattern write-combining is good at.
 */
static int make_buf(struct kkms_out *o, int i)
{
	struct drm_mode_create_dumb create = { 0 };
	struct drm_mode_map_dumb map = { 0 };

	create.width = o->width;
	create.height = o->height;
	create.bpp = 32;

	if (drmIoctl(K.drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0)
		return fail_with("create dumb buffer", strerror(errno));

	o->handle[i] = create.handle;
	o->stride = create.pitch;
	o->size = create.size;

	if (drmModeAddFB(K.drm_fd, o->width, o->height, 24, 32, o->stride,
			 o->handle[i], &o->fb[i]) != 0)
		return fail_with("drmModeAddFB", strerror(errno));

	map.handle = o->handle[i];
	if (drmIoctl(K.drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0)
		return fail_with("map dumb buffer", strerror(errno));

	o->pixels[i] = mmap(NULL, o->size, PROT_READ | PROT_WRITE, MAP_SHARED,
			    K.drm_fd, (off_t)map.offset);
	if (o->pixels[i] == MAP_FAILED) {
		o->pixels[i] = NULL;
		return fail_with("mmap the framebuffer", strerror(errno));
	}

	memset(o->pixels[i], 0, o->size);
	return 0;
}

/*
 * ONE SCANOUT BUFFER, GIVEN BACK.
 *
 * Written once and used by both the teardown and the fallback that abandons a
 * buffer it could not finish: make_buf() fills the handle and the framebuffer
 * id before it can fail at the mapping, and a slot merely zeroed leaves a
 * whole screen of GPU memory that nothing can reach again, because the fields
 * are the only record of it.
 *
 * `stride` and `size` describe EVERY slot and are left alone: the other
 * buffers are still mapped with them.
 */
static void buf_free(struct kkms_out *o, int i)
{
	if (o->pixels[i]) {
		munmap(o->pixels[i], o->size);
		o->pixels[i] = NULL;
	}
	if (o->fb[i]) {
		drmModeRmFB(K.drm_fd, o->fb[i]);
		o->fb[i] = 0;
	}
	if (o->handle[i]) {
		struct drm_mode_destroy_dumb d = { .handle = o->handle[i] };

		drmIoctl(K.drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
		o->handle[i] = 0;
	}
}

/*
 * A BUFFER NOTHING IS READING AND NOTHING IS WAITING FOR, or -1.
 *
 * The roles are the whole of the safety argument: `front` is under the raster,
 * `queued` is the one the kernel will show next, `ready` already holds a
 * composed frame. Anything else is the painter's. -1 means every buffer is
 * spoken for and this screen takes no frame this time round.
 *
 * ONE BUFFER IS THE EXCEPTION AND ANSWERS 0. That path paints in place, into
 * the buffer the CRTC is reading, and is taken only where that cannot tear —
 * a driver that uploads at a dirty rectangle — or where there was no second
 * buffer to be had and a torn screen beats no screen.
 */
static int pick_free(const struct kkms_out *o)
{
	if (o->nbuf < 2)
		return o->nbuf == 1 ? 0 : -1;
	for (int i = 0; i < o->nbuf; i++)
		if (i != o->front && i != o->queued && i != o->ready)
			return i;
	return -1;
}

static void flip_reset(struct kkms_out *o)
{
	o->queued = -1;
	o->ready = -1;
	o->flip_pending = 0;
	o->back = pick_free(o);
	/*
	 * THE GENERATION MOVES WITH EVERY ABANDONMENT. A flip the kernel still
	 * has queued reports later, and a completion taken for a live one
	 * names its buffer as the front one — so the next paint goes into the
	 * buffer the raster is inside.
	 */
	o->flip_gen++;
}

static int make_fb(struct kkms_out *o)
{
	int want = K.want_bufs;

	if (make_buf(o, 0) != 0)
		return -1;
	/*
	 * MORE THAN ONE BUFFER IS WANTED, NOT REQUIRED, AND NOT ALWAYS WANTED.
	 *
	 * A driver with no memory for another, and a machine with several
	 * large screens, still get a desktop: `nbuf` says which present is in
	 * force. A transfer-model driver is not even asked — there a flip
	 * costs the whole plane and buys nothing, because the host never reads
	 * the buffer except at the copy the dirty rectangle triggers.
	 *
	 * EACH EXTRA BUFFER IS ASKED FOR IN TURN AND THE FIRST REFUSAL ENDS
	 * IT. A card that gave a second and refused a third is a card with two
	 * buffers, not a failure: 8 MB a screen at 1080p is a real ceiling on
	 * a machine driving several of them, and a desktop that will not come
	 * up because a third buffer would not fit is the worst of the answers
	 * available.
	 */
	if (want < 1 || want > KKMS_NBUF)
		want = KKMS_NBUF;
	o->nbuf = 1;
	if (!K.drm_transfers)
		while (o->nbuf < want) {
			if (make_buf(o, o->nbuf) != 0) {
				buf_free(o, o->nbuf);
				/* Not a failure, and not reported as one. */
				reason[0] = '\0';
				break;
			}
			o->nbuf++;
		}
	o->front = 0;
	o->queued = -1;
	o->ready = -1;
	o->flip_pending = 0;
	o->back = pick_free(o);
	/* New buffers are a new flip identity: a completion still in the
	 * kernel's queue names the buffers that are gone. */
	o->flip_gen++;
	for (int i = 0; i < KKMS_NBUF; i++)
		o->pad_owed[i] = 0;
	o->buf_conn = o->connector;
	o->buf_w = o->width;
	o->buf_h = o->height;
	o->buf_mode = o->mode;

	free(o->shadow_bits);
	o->shadow_bits = calloc(1, (size_t)o->stride * (size_t)o->height);
	if (!o->shadow_bits)
		return fail_with("the painter's buffer", "out of memory");

	if (o->image)
		pixman_image_unref(o->image);
	o->image = pixman_image_create_bits(PIXMAN_x8r8g8b8, o->width,
					    o->height, o->shadow_bits,
					    (int)o->stride);
	if (!o->image)
		return fail_with("pixman_image_create_bits", "out of memory");

	return 0;
}

/*
 * THE BUFFER THE CRTC IS SCANNING OUT, which is a ROLE and not arithmetic on
 * the painter's index: with three buffers there is no "the other one", and a
 * modeset pointed at the wrong buffer shows a frame that was never painted.
 */
static uint32_t front_fb(const struct kkms_out *o)
{
	return o->front >= 0 && o->front < o->nbuf ? o->fb[o->front] : 0;
}

/*
 * WHICH FLIP A COMPLETION BELONGS TO, carried in the event's own cookie.
 *
 * A pointer into K.out[] does not identify one: the kernel delivers a
 * completion for a framebuffer that has since been freed, and a re-probe
 * gives the same slot to another connector. So the cookie is the slot and the
 * generation the flip was issued under, and an event whose generation has
 * moved on is dropped. Clearing `flip_pending` on a stale event is not a
 * cosmetic error — the next flush would paint and copy into the buffer the
 * raster is inside.
 */
static void *flip_cookie(const struct kkms_out *o)
{
	unsigned slot = (unsigned)(o - K.out);

	return (void *)(uintptr_t)((slot << 16) | (o->flip_gen & 0xffff));
}

/*
 * ONE FLIP, ISSUED. Returns 0, or the errno that refused it.
 *
 * THE TEARING FLAG IS DROPPED ON ITS FIRST REFUSAL AND FOR THE REST OF THE
 * SESSION. A driver that publishes DRM_CAP_ASYNC_PAGE_FLIP and then answers
 * EINVAL for a flip carrying DRM_MODE_PAGE_FLIP_ASYNC — a mode it cannot tear
 * in, a plane configuration it will not — is refusing the FLAG and not the
 * flip, and EINVAL is the one errno the caller reads as "this driver cannot
 * flip" and answers by giving up every buffer but one. So the flip is issued
 * again without it before any verdict is reached, and a screen loses its
 * latency rather than its buffers.
 *
 * libdrm's ioctl wrapper returns -errno on some paths and -1 on others, so the
 * sign is not relied on.
 */
static int do_flip(struct kkms_out *o, int buf)
{
	int rc = drmModePageFlip(K.drm_fd, o->crtc, o->fb[buf],
				 DRM_MODE_PAGE_FLIP_EVENT | K.flip_flags,
				 flip_cookie(o));
	int e;

	if (rc == 0)
		return 0;
	e = rc < 0 && rc != -1 ? -rc : errno;
	if (K.flip_flags && e == EINVAL) {
		K.flip_flags = 0;
		rc = drmModePageFlip(K.drm_fd, o->crtc, o->fb[buf],
				     DRM_MODE_PAGE_FLIP_EVENT,
				     flip_cookie(o));
		if (rc == 0)
			return 0;
		e = rc < 0 && rc != -1 ? -rc : errno;
	}
	return e ? e : EIO;
}

/*
 * THE FRAME COMPOSED WHILE THE LAST FLIP WAS IN FLIGHT, PUT ON THE SCREEN.
 *
 * ISSUED FROM THE COMPLETION AND NOT FROM THE NEXT FLUSH. The flush runs on
 * the caller's cadence and the vblank does not, so a frame held until the
 * caller next visits is a frame presented a refresh period late — and that
 * period is the whole of what the third buffer buys.
 *
 * NOTHING IS PRESENTED WHILE THE SCREEN IS DARK OR THE SESSION IS AWAY. The
 * CRTC is detached or somebody else's, and a flip issued against it either
 * relights a screen the session believes is asleep or is refused; the composed
 * frame is kept either way, and the repaint those paths already owe replaces
 * it.
 */
static void present_ready(struct kkms_out *o)
{
	if (o->ready < 0 || o->queued >= 0 || o->nbuf < 2)
		return;
	if (!K.active || K.blanked || K.drm_fd < 0)
		return;
	/*
	 * A REFUSAL HERE LEAVES THE FRAME WHERE IT IS. It is the transient
	 * kind — a CRTC another VT holds, one a blank detached — because a
	 * driver's own refusal has already taken this output off the flip path
	 * at the flush, and `ready` is only ever set on an output that flipped.
	 */
	if (do_flip(o, o->ready) != 0)
		return;
	o->queued = o->ready;
	o->ready = -1;
	o->flip_pending = 1;
	if (o->back < 0)
		o->back = pick_free(o);
}

/*
 * THE FLIP COMPLETED. The buffer it named is what the CRTC is scanning out
 * now, the one it replaced is the painter's again, and `owed` already says
 * which of that buffer's rows are frames behind.
 */
static void on_flip(int fd, unsigned seq, unsigned sec, unsigned usec,
		    void *data)
{
	unsigned tag = (unsigned)(uintptr_t)data;
	unsigned slot = tag >> 16;
	struct kkms_out *o;

	(void)fd;
	(void)seq;
	(void)sec;
	(void)usec;
	/*
	 * BOUNDED BY THE ARRAY, NOT BY THE COUNT. `nout` is how many outputs
	 * are lit, and a slot above it may still hold buffers and an
	 * outstanding flip — a probe that finds no monitor sets nout to 0 and
	 * deliberately keeps them. Discarding the completion there leaves
	 * flip_pending set for ever, and the screen that comes back is never
	 * painted again. The generation below is what tells a stale event
	 * from a live one.
	 */
	if (slot >= KKMS_MAX_OUT)
		return;
	o = &K.out[slot];
	if ((o->flip_gen & 0xffff) != (tag & 0xffff))
		return;
	/*
	 * THE FRONT MOVES HERE AND NOWHERE ELSE on the flip path. Moving it
	 * when the flip was ISSUED would name a buffer the raster has not
	 * reached yet, and every relight and every wake points the CRTC at
	 * front_fb().
	 */
	if (o->queued >= 0)
		o->front = o->queued;
	o->queued = -1;
	o->flip_pending = 0;
	if (o->back < 0)
		o->back = pick_free(o);
	present_ready(o);
}

/*
 * THE SLICE, GIVEN BACK, LEAVING AN OUTPUT kkms_flush() SKIPS.
 *
 * The flush's guard is `image && cur && prev && painted`, and nothing below
 * it re-tests the row arrays, so a slice that is half built must present as
 * no slice at all: an output whose `painted` survives a failed recut is
 * walked with row and column counts lay_out() has already moved, and the
 * copy runs off the end of `cur` and through a null `owed`.
 */
static void slice_free(struct kkms_out *o)
{
	free(o->painted);
	o->painted = NULL;
	for (int i = 0; i < KKMS_NBUF; i++) {
		free(o->owed[i]);
		o->owed[i] = NULL;
		o->pad_owed[i] = 0;
	}
	free(o->cur);
	free(o->prev);
	o->cur = o->prev = NULL;
	o->ncell = 0;
}

/*
 * THIS OUTPUT'S SLICE OF THE SHARED GRID, and the frame it last painted.
 *
 * Allocated per output and never shared: `kcell_paint` decides which rows to
 * repaint by comparing `cur` against `prev`, so two screens sharing one `prev`
 * would each find the other's paint already done and neither would redraw.
 *
 * A FAILURE LEAVES NO SLICE AT ALL, so the caller may report it and leave the
 * output in K.nout: the flush's own guard then skips that screen instead of
 * walking a half-built one.
 */
static int make_slice(struct kkms_out *o)
{
	int n = o->cols * o->rows;

	if (n <= 0) {
		slice_free(o);
		return fail_with("the output's grid", "no cells");
	}
	if (o->ncell != n) {
		free(o->cur);
		free(o->prev);
		o->cur = calloc((size_t)n, sizeof(*o->cur));
		o->prev = calloc((size_t)n, sizeof(*o->prev));
		o->ncell = o->cur && o->prev ? n : 0;
		if (!o->ncell) {
			slice_free(o);
			return fail_with("the output's grid", "out of memory");
		}
	}
	free(o->painted);
	o->painted = calloc((size_t)o->rows, 1);
	if (!o->painted) {
		slice_free(o);
		return fail_with("the output's grid", "out of memory");
	}
	/*
	 * A ROW DEBT PER BUFFER, FOR EVERY SLOT AND NOT FOR `nbuf` OF THEM.
	 * make_fb() may raise nbuf — a re-probe that could not spare a third
	 * buffer and a later one that could — and a buffer that came back with
	 * no `owed` array is a screen the flush skips for the rest of the
	 * session, because its guard is the array and not the count.
	 */
	for (int i = 0; i < KKMS_NBUF; i++) {
		free(o->owed[i]);
		o->owed[i] = calloc((size_t)o->rows, 1);
		if (!o->owed[i]) {
			slice_free(o);
			return fail_with("the output's grid", "out of memory");
		}
		/* A different number of rows puts the strip below them
		 * somewhere else, so every buffer owes it. */
		o->pad_owed[i] = 1;
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
	free(o->shadow_bits);
	o->shadow_bits = NULL;
	for (int i = 0; i < KKMS_NBUF; i++)
		buf_free(o, i);
	slice_free(o);
	o->nbuf = 0;
	o->front = 0;
	o->back = 0;
	o->queued = -1;
	o->ready = -1;
	o->flip_pending = 0;
	/* The buffers a flip in flight named are gone; its completion must
	 * not clear the flag belonging to whatever this slot becomes. */
	o->flip_gen++;
	o->buf_conn = 0;
	o->buf_w = o->buf_h = 0;
	memset(&o->buf_mode, 0, sizeof(o->buf_mode));
}

/*
 * LAY THE SCREENS OUT AND LIGHT THEM, from whatever the modes now say.
 *
 * The one place buffers are made and CRTCs are programmed, so a hotplug and a
 * mode change take the same path — two of them would be two answers to what a
 * screen is, and the second one would be wrong the first time a field was
 * added. Returns 0, or -1 having left as many outputs lit as it managed.
 */
/*
 * GIVE UP FROM `first` ON, RELEASING WHAT THOSE OUTPUTS HOLD.
 *
 * Truncating nout alone strands every later output's dumb buffer, its DRM
 * framebuffer, its mapping and its slices: nothing iterates past nout again,
 * so the shutdown, the reprobe and the rollback relight all walk a shorter
 * list and the memory is gone until the process is.
 */
static void drop_outputs_from(int first)
{
	for (int k = first; k < K.nout; k++)
		out_free(&K.out[k]);
	K.nout = first;
}

/*
 * THE SAME TIMING, FIELD BY FIELD AND NOT BYTE BY BYTE.
 *
 * drmModeModeInfo carries a 32-byte name that a driver may spell differently
 * for a timing it publishes identically, so a memcmp would call two identical
 * modes different and re-program the CRTC — a black frame for nothing — on
 * every hotplug.
 */
static int mode_same(const drmModeModeInfo *a, const drmModeModeInfo *b)
{
	return a->clock == b->clock &&
	       a->hdisplay == b->hdisplay && a->hsync_start == b->hsync_start &&
	       a->hsync_end == b->hsync_end && a->htotal == b->htotal &&
	       a->vdisplay == b->vdisplay && a->vsync_start == b->vsync_start &&
	       a->vsync_end == b->vsync_end && a->vtotal == b->vtotal &&
	       a->vscan == b->vscan && a->vrefresh == b->vrefresh &&
	       a->flags == b->flags;
}

static int relight(void)
{
	lay_out();
	for (int i = 0; i < K.nout; i++) {
		/*
		 * A SCREEN WHOSE PIXELS DID NOT MOVE KEEPS ITS BUFFERS. This
		 * runs for every output on any hotplug — a second monitor
		 * waking is one — and tearing down a framebuffer that is
		 * being scanned out disables its CRTC: every other screen went
		 * black and came back because one of them changed.
		 *
		 * The grid it shows may still have moved, so the slice is
		 * remade and the whole screen repainted; that is a frame, not
		 * a blank.
		 *
		 * THE CRTC IS A SEPARATE DECISION FROM THE BUFFERS. Two modes
		 * of the same size differ in their timing, and only the
		 * modeset puts a timing in force: without this, choosing
		 * 1920x1080@144 while on 1920x1080@60 keeps the buffers, keeps
		 * the old refresh rate and reports success.
		 */
		struct kkms_out *o = &K.out[i];
		int keep = o->pixels[0] && o->buf_conn == o->connector &&
			   o->buf_w == o->width && o->buf_h == o->height;

		if (keep) {
			if (make_slice(o) != 0) {
				drop_outputs_from(i);
				return -1;
			}
			owe_all(o);
			if (mode_same(&o->buf_mode, &o->mode))
				continue;
			if (drmModeSetCrtc(K.drm_fd, o->crtc, front_fb(o), 0,
					   0, &o->connector, 1,
					   &o->mode) != 0)
				return -1;
			o->buf_mode = o->mode;
			o->force_full = 1;
			/*
			 * A MODESET CANCELS A FLIP THE KERNEL HAS NOT YET
			 * REPORTED, and the modeset itself put `front` on the
			 * screen. kkms_flush() skips an output with no free
			 * buffer, so a queue left standing here freezes this
			 * screen for the rest of the session — and the
			 * generation moves with it inside flip_reset(), or
			 * the cancelled flip's completion names its buffer as
			 * the front one and the next paint goes into the one
			 * the raster is inside.
			 */
			flip_reset(o);
			continue;
		}

		/*
		 * WHAT THIS OUTPUT ALREADY HAD GOES FIRST. make_fb() and
		 * make_slice() overwrite every field they fill, so without
		 * this each mode change leaks the previous mapping, its GEM
		 * handle, its DRM framebuffer and its pixman image — a screen
		 * whose mode is stepped a few times has leaked several
		 * framebuffers' worth of a device the session cannot get back.
		 * out_free() is written to take a half-built output.
		 */
		out_free(o);
		if (make_fb(o) != 0 || make_slice(o) != 0) {
			drop_outputs_from(i);
			return -1;
		}
		if (drmModeSetCrtc(K.drm_fd, K.out[i].crtc, front_fb(&K.out[i]),
				   0, 0, &K.out[i].connector, 1,
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

	if (K.res)
		drmModeFreeResources(K.res);
	K.res = drmModeGetResources(K.drm_fd);
	if (!K.res || pick_outputs() != 0) {
		/*
		 * Nothing came back. The buffers are kept rather than freed:
		 * a machine that has just lost its only monitor keeps the
		 * desktop it had, because the monitor may come back and the
		 * session is running either way.
		 */
		K.nout = 0;
		return 0;
	}

	/*
	 * THE BUFFERS ARE NOT TORN DOWN UP FRONT. Freeing a framebuffer that
	 * is being scanned out disables its CRTC, so a hotplug on any
	 * connector blanked every screen on the machine for as long as it took
	 * to remake them all. relight() below keeps the ones whose connector
	 * and mode came back the same and remakes only the rest; a slot the
	 * new probe does not reach is freed here.
	 *
	 * THE SWEEP RUNS TO THE END OF THE ARRAY, not to the count `on` holds.
	 * A probe that found nothing returns above with K.nout = 0 and the
	 * slots still populated, so K.nout is not a record of what is held;
	 * bounding the sweep by it would strand every slot above the new
	 * count for the rest of the session. out_free() takes a slot that was
	 * never built.
	 */
	for (int i = K.nout; i < KKMS_MAX_OUT; i++)
		out_free(&K.out[i]);

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
	/* THE TIMING IN FORCE, not the one a picker listed: a mode change and
	 * a hotplug both move it, and a session pacing itself to a number it
	 * read once paces to a screen that is no longer there. */
	out->refresh = mode_refresh_mhz(&K.out[i].mode);
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
	/* MILLIHERTZ, AND COMPUTED RATHER THAN READ — one arithmetic, shared
	 * with the picker, so a mode a person chooses from this list is the
	 * one the picker would have ranked. */
	m->refresh = mode_refresh_mhz(d);
	m->preferred = (d->type & DRM_MODE_TYPE_PREFERRED) != 0;
	return 1;
}

int kkms_mode_current(int out)
{
	return out >= 0 && out < K.nout ? K.out[out].cur_mode : -1;
}

/*
 * THE RATE TO PACE A SESSION AT: the fastest screen that is lit.
 *
 * The fastest and not an average, because one grid is cut across every screen.
 * A frame slow enough for the 60 Hz panel is a frame the 144 Hz one shows
 * twice; the other direction costs a frame the slower screen never scans out,
 * which is a frame and not a stall.
 */
int kkms_refresh_mhz(void)
{
	int best = 0;

	for (int i = 0; i < K.nout; i++) {
		int r = mode_refresh_mhz(&K.out[i].mode);

		if (r > best)
			best = r;
	}
	return best;
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
	/*
	 * A CHANGE THAT ARRIVED WHILE SWITCHED AWAY IS HELD, NOT DROPPED. The
	 * device is not ours to re-probe then, but the event is the only
	 * notice there will be: a monitor plugged in or unplugged on another
	 * VT was simply never noticed, and the desktop came back the size of
	 * a screen that had gone.
	 */
	if (changed && !K.active) {
		K.hotplug_pending = 1;
		return 0;
	}
	if (K.active && (changed || K.hotplug_pending)) {
		K.hotplug_pending = 0;
		return reprobe();
	}
	return 0;
}

/* ── the backend ─────────────────────────────────────────────────────── */

/*
 * THE SCANOUT BUFFER IS WRITTEN BY THE CPU AND THE DRIVER HAS TO BE TOLD.
 *
 * This is a dumb buffer: painting goes into a mapping of guest memory, and a
 * card that scans that memory out directly needs nothing more. A driver that
 * uploads the buffer to a host does — see driver_transfers() — and there the
 * rectangle IS the bandwidth: a whole 1920x1080 screen is 8 MB where one
 * changed row is a few tens of kilobytes.
 *
 * THE RECTANGLE IS THE ROWS THIS FRAME PAINTED, in runs, so a clock ticking
 * in a corner costs its own corner. On the single-buffer path — the one the
 * uploading drivers take — those are exactly the rows that reached the
 * buffer. On the flip path a buffer may also have been given rows it owed
 * from an earlier frame and the rectangle does not name them, which is
 * harmless only because a flip presents the whole buffer — and is a second
 * reason a card that uploads is kept off the flip path.
 *
 * A driver with no dirty hook answers EINVAL, which is not a failure here —
 * it is a card that already showed the pixels.
 *
 * ONLY THE SINGLE-BUFFER PATH ASKS. On the flip path the rectangle says
 * nothing a flip has not already done, and asking costs a vblank: the hook is
 * `drm_atomic_helper_dirtyfb` on every driver that uploads, and it ends in a
 * blocking drm_atomic_commit that waits for the flip already in flight.
 */
static void kkms_dirty(const struct kkms_out *o, uint32_t fb, int ch)
{
	drmModeClip clips[16];
	int n = 0;

	for (int y = 0; y < o->rows && n < (int)(sizeof(clips) /
						 sizeof(clips[0]));) {
		if (!o->painted[y]) {
			y++;
			continue;
		}

		int start = y;

		while (y < o->rows && o->painted[y])
			y++;

		int y0 = start * ch;
		int y1 = y * ch;

		/*
		 * A RUN THAT REACHES THE LAST ROW REACHES THE BOTTOM OF THE
		 * MODE. `painted` is one byte per CELL row, so the tallest
		 * rectangle it can name ends at rows*ch and the strip below
		 * it — the pixels `pad_owed` writes, where the mode is not a
		 * whole number of cells — would never be named to a host that
		 * only reads what a clip covers. Extending the last run is
		 * what shows it; widening every clip would send the strip on
		 * every partial frame for nothing.
		 */
		if (y == o->rows)
			y1 = o->height;
		clips[n++] = (drmModeClip){
			.x1 = 0, .y1 = (unsigned short)y0,
			.x2 = (unsigned short)o->width,
			.y2 = (unsigned short)y1,
		};
	}

	/* More runs than the array holds is a frame that changed all over, and
	 * one rectangle over the whole screen is cheaper to send than to
	 * split further. */
	if (n == (int)(sizeof(clips) / sizeof(clips[0])))
		clips[0] = (drmModeClip){ 0, 0, (unsigned short)o->width,
					  (unsigned short)o->height },
		n = 1;

	if (n)
		drmModeDirtyFB(K.drm_fd, fb, clips, n);
}

/* Every row of this output is a frame behind in every buffer it holds: a full
 * repaint, a mode change, or coming back from another VT. */
static void owe_all(struct kkms_out *o)
{
	for (int i = 0; i < o->nbuf; i++)
		if (o->owed[i])
			memset(o->owed[i], 1, (size_t)o->rows);
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
 *
 * THE FRAME IS ASSEMBLED IN SYSTEM MEMORY AND PRESENTED AS A FLIP. Nothing
 * writes into a buffer the kernel can see: the painter draws into the shadow,
 * the rows it touched are copied into a buffer that is neither on the screen
 * nor named by a flip, and the CRTC is pointed at that buffer at the next
 * vblank. A row caught by the raster mid-paint is what tearing IS, and there
 * is no moment when one could be — unless the caller asked for tearing, which
 * changes when the CRTC is re-pointed and nothing else.
 *
 * A THIRD BUFFER IS WHAT LETS THIS RUN WHILE A FLIP IS STILL IN FLIGHT. The
 * composed frame is held in `ready` and the flip's own completion issues its
 * flip, so a compose that overruns a refresh period costs that frame's latency
 * and not the next frame's whole period. With two buffers there is nothing to
 * paint into and the screen is skipped until the flip retires.
 *
 * A TRANSFER-MODEL DRIVER GETS THE SINGLE BUFFER INSTEAD, and gets it without
 * tearing: nothing reads that buffer until drmModeDirtyFB names the rows to
 * upload, so the changed rows are the whole cost of the frame, where a flip
 * there would upload the entire plane. Which model a card is, is settled once
 * at open; see driver_transfers().
 */
/*
 * CELLS THAT OWE A REPAINT THOUGH THEIR BYTES DID NOT CHANGE, spoiled in THIS
 * BACKEND'S OWN previous frame — the one the diff below actually reads.
 *
 * A sprite cell names a slot rather than carrying a picture, so a new picture
 * in the same slot writes an identical cell and the row compare finds nothing;
 * without this an embedded application would hold its first frame for ever
 * while every other part of the path worked. Spoiling libktui's copy cannot
 * serve: it is per session where this is per screen, and the flush below
 * overwrites it from `cur` before the diff runs.
 *
 * The rectangle is in the shared grid's columns, so each screen takes the part
 * that falls in its own slice — `o->col` is where that slice starts.
 */
static void kkms_owe(int x, int y, int w, int h)
{
	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		if (!o->prev || o->cols < 1 || o->rows < 1)
			continue;

		int x0 = x - o->col, x1 = x + w - o->col;
		int y0 = y, y1 = y + h;

		if (x0 < 0)
			x0 = 0;
		if (y0 < 0)
			y0 = 0;
		if (x1 > o->cols)
			x1 = o->cols;
		if (y1 > o->rows)
			y1 = o->rows;

		for (int r = y0; r < y1; r++)
			for (int c = x0; c < x1; c++)
				o->prev[(size_t)r * o->cols + c].ch = 0xffffffffu;
	}
}

/* ── the pointer ──────────────────────────────────────────────────────────
 *
 * AN ARROW IN PIXELS, COMPOSITED INTO THE SHADOW AFTER THE CELLS.
 *
 * It is not a cell and cannot be one: `libktui` draws the pointer as the cell
 * under it reversed, which is the only pointer a terminal, a dump and a
 * braille display can show, and a screen with a framebuffer under it can do
 * better. This library answers `KtuiBackend.pointer` and takes the job; every
 * other backend leaves the entry NULL and keeps the reversed cell.
 *
 * NOT A HARDWARE CURSOR PLANE. `drmModeSetCursor2` would move the arrow
 * without repainting anything, and it is a separate plane with its own size
 * limits, its own format and a per-driver set of refusals — see
 * known-gaps.md. The software arrow costs the rows it covers and works on
 * every driver, including the transfer-model ones that have no plane at all.
 * ──────────────────────────────────────────────────────────────────────── */

/*
 * THE ARROW, DRAWN IN CODE AND NOT LOADED FROM ANYWHERE. This library opens a
 * GPU device and nothing else; a cursor read from a file would be a path, a
 * failure mode and a theme this library has no business having.
 *
 * `X` is the body. The OUTLINE IS NOT IN THE TABLE — it is every blank cell
 * of the eight-neighbourhood of a body pixel, computed below, because an
 * outline drawn by hand is an outline with a hole in it the first time the
 * shape is edited, and a hole is exactly where the arrow disappears into
 * same-coloured content.
 *
 * The tip is (0,0) and the shape is the one every system draws: a vertical
 * left edge, a 45-degree upper-right edge, and a tail leaving the notch at
 * the bottom.
 */
#define KKMS_PTR_W 11
#define KKMS_PTR_H 18

static const char ptr_mask[KKMS_PTR_H][KKMS_PTR_W + 1] = {
	"X..........",
	"XX.........",
	"XXX........",
	"XXXX.......",
	"XXXXX......",
	"XXXXXX.....",
	"XXXXXXX....",
	"XXXXXXXX...",
	"XXXXXXXXX..",
	"XXXXXXXXXX.",
	"XXXXXXXXXXX",
	"XXXXXXX....",
	"XXXXXXX....",
	"XXXX.XXX...",
	"XXX..XXX...",
	"XX....XXX..",
	"X.....XXX..",
	".......XXX.",
};

/*
 * WHERE THE ARROW'S TIP IS, IN THE SHARED GRID'S PIXELS, and where each screen
 * last put it. A negative x is no pointer at all.
 *
 * PIXELS AND NOT CELLS. The session names a cell because a cell is the whole
 * of what it routes on, but this library reads the device itself and the arrow
 * is its own pixels: an arrow that stepped a character at a time is a pointer
 * that cannot be put on a scrollbar two pixels wide, and it reads as a stutter
 * against a hand that is moving smoothly.
 *
 * Per screen because a screen that was skipped — no free buffer, a flip in
 * flight — still holds the arrow it was last given, and a single record would
 * tell it the arrow it is still showing had already been taken off.
 */
static int ptr_x = -1, ptr_y = -1;
static int ptr_last_x[KKMS_MAX_OUT], ptr_last_y[KKMS_MAX_OUT];
static unsigned char ptr_last_on[KKMS_MAX_OUT];

/*
 * WHOLE PIXELS PER MASK PIXEL, so the arrow is about a cell tall at every font
 * size — the same footprint the reversed cell has, which is what keeps the
 * pointer the same size when a session is looked at through two views at once.
 *
 * INTEGER, never a resample. An 11-pixel-wide arrow scaled by a fraction loses
 * its one-pixel outline on whichever rows round the same way twice, and an
 * outline with a gap in it is the whole of what the outline is for.
 */
static int ptr_scale(void)
{
	int s = (kcell_h() + KKMS_PTR_H / 2) / KKMS_PTR_H;

	return s < 1 ? 1 : s;
}

static int ptr_body(int x, int y)
{
	return x >= 0 && x < KKMS_PTR_W && y >= 0 && y < KKMS_PTR_H &&
	       ptr_mask[y][x] == 'X';
}

static int ptr_edge(int x, int y)
{
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (ptr_body(x + dx, y + dy))
				return 1;
	return 0;
}

/*
 * THE PIXELS THE ARROW COVERS ON ONE SCREEN, in that screen's own pixels and
 * already clipped to it, or 0 for a pointer that is not on this screen at all.
 *
 * The tip is the pixel the device is at. The outline puts one scaled pixel
 * outside the mask on every side, which is why the box starts a scale step
 * above and to the left of the tip.
 *
 * CLIPPED TO THE WHOLE CELLS, not to the mode. The strip below the last row
 * and the one right of the last column are written by the painter only on a
 * full repaint and no row of `owed` covers them, so an arrow drawn there would
 * reach the shadow and no buffer.
 */
static int ptr_box(const struct kkms_out *o, int px, int py,
		   int *x0, int *y0, int *x1, int *y1)
{
	int cw = kcell_w(), ch = kcell_h(), s = ptr_scale();

	if (px < 0 || py < 0 || cw < 1 || ch < 1)
		return 0;
	*x0 = px - o->col * cw - s;
	*y0 = py - s;
	*x1 = *x0 + (KKMS_PTR_W + 2) * s;
	*y1 = *y0 + (KKMS_PTR_H + 2) * s;
	if (*x0 < 0)
		*x0 = 0;
	if (*y0 < 0)
		*y0 = 0;
	if (*x1 > o->cols * cw)
		*x1 = o->cols * cw;
	if (*y1 > o->rows * ch)
		*y1 = o->rows * ch;
	return *x1 > *x0 && *y1 > *y0;
}

/*
 * PUT THE CELLS UNDER A BOX BACK IN THE DIFF, which is how the arrow is
 * ERASED. Nothing in the cell model knows the arrow is there: the cells it
 * covers are unchanged, so the row compare finds nothing and the painter
 * leaves the arrow's pixels where they are. Spoiling this screen's own
 * previous frame is what makes the paint below repaint them — without it the
 * pointer leaves a trail of arrows behind it, one per place it stopped.
 */
static void ptr_spoil(struct kkms_out *o, int x0, int y0, int x1, int y1)
{
	int cw = kcell_w(), ch = kcell_h();

	for (int r = y0 / ch; r < o->rows && r <= (y1 - 1) / ch; r++)
		for (int c = x0 / cw; c < o->cols && c <= (x1 - 1) / cw; c++)
			o->prev[(size_t)r * o->cols + c].ch = 0xffffffffu;
}

/* A slot as this framebuffer's pixel. The shadow is PIXMAN_x8r8g8b8 and the
 * arrow is two opaque colours, so the eight-bit channels go straight in —
 * there is nothing to composite and no alpha to carry. */
static uint32_t ptr_pixel(int slot)
{
	pixman_color_t c = kcell_slot_color(slot);

	return ((uint32_t)(c.red >> 8) << 16) |
	       ((uint32_t)(c.green >> 8) << 8) | (uint32_t)(c.blue >> 8);
}

/*
 * THE BODY IS KT_TEXT AND THE OUTLINE KT_BG, not the other way round.
 *
 * Most of any screen is the background slot, and the body is the whole area of
 * the arrow: filled in the foreground it is legible over all of that without
 * relying on one pixel of anything. The outline is what rescues it over the
 * minority that is foreground-coloured — a reversed selection, a filled title
 * band, a bright chart bar. Filling the body in KT_BG instead would leave the
 * arrow invisible over most of the desktop with a one-pixel line standing for
 * it, and a one-pixel line is what a scaled-down screenshot loses first.
 *
 * SLOTS AND NOT LITERALS, so `kdos theme` and night light reach the pointer
 * like everything else: a light theme draws a dark arrow with a light outline
 * without a second decision being made anywhere.
 */
static void ptr_draw(struct kkms_out *o, int px, int py)
{
	int cw = kcell_w(), ch = kcell_h(), s = ptr_scale();
	int ox = px - o->col * cw, oy = py;
	int maxx = o->cols * cw, maxy = o->rows * ch;
	uint32_t body = ptr_pixel(KT_TEXT), edge = ptr_pixel(KT_BG);

	for (int my = -1; my <= KKMS_PTR_H; my++) {
		for (int mx = -1; mx <= KKMS_PTR_W; mx++) {
			uint32_t v;

			if (ptr_body(mx, my))
				v = body;
			else if (ptr_edge(mx, my))
				v = edge;
			else
				continue;

			for (int dy = 0; dy < s; dy++) {
				int y = oy + my * s + dy;
				uint32_t *row;

				if (y < 0 || y >= maxy)
					continue;
				row = (uint32_t *)((unsigned char *)
						   o->shadow_bits +
						   (size_t)y * o->stride);
				for (int dx = 0; dx < s; dx++) {
					int x = ox + mx * s + dx;

					if (x >= 0 && x < maxx)
						row[x] = v;
				}
			}
		}
	}
}

/*
 * THIS BACKEND CLAIMS THE POINTER. See KtuiBackend.pointer: answering 1 is a
 * promise that the cells reach the screen exactly as the session composed them
 * and the arrow is in the pixels under them, which kkms_flush() below keeps.
 *
 * IT CLAIMS IT EVEN WHEN NO SCREEN IS BEING PAINTED — switched away, blanked,
 * every buffer spoken for. Declining there would put a reversed cell into the
 * frame the session is accumulating, and that frame is what comes back when
 * the screen does: the repaint is whole, so the arrow arrives with it.
 */
static int kkms_pointer(int x, int y)
{
	int cw = kcell_w(), ch = kcell_h();

	if (x < 0 || y < 0 || cw < 1 || ch < 1) {
		ptr_x = ptr_y = -1;
		return 1;
	}

	/*
	 * THE DEVICE'S OWN PIXEL WHERE IT AGREES WITH THE NAMED CELL, and the
	 * cell's corner where it does not.
	 *
	 * The two agree for a mouse: the cell the session routed on was derived
	 * from this very position. They disagree for a pointer this library did
	 * not move — a finger, whose cell comes from the touch recogniser and
	 * leaves the device position where the mouse last was — and there the
	 * cell is the only true answer, so the arrow snaps to it.
	 */
	if (K.ptr_seen && (int)K.ptr_px / cw == x && (int)K.ptr_py / ch == y) {
		ptr_x = (int)K.ptr_px;
		ptr_y = (int)K.ptr_py;
	} else {
		ptr_x = x * cw;
		ptr_y = y * ch;
	}
	return 1;
}

static void kkms_flush(const KtuiCell *cur, KtuiCell *prev, int w, int h,
		       int force_full)
{
	const int ch = kcell_h();

	if (!K.active || K.blanked)
		return;

	/*
	 * THE TOOLKIT'S OWN LAST-PRESENTED BUFFER IS MAINTAINED, because
	 * something else reads it: the sprite table asks ktui_cells() whether
	 * a picture is still on the screen before it takes the slot back, and
	 * a backend that left that buffer as the zeroes it was allocated as
	 * would have every picture on this display look evictable.
	 */
	if (prev)
		memcpy(prev, cur, (size_t)w * h * sizeof(*cur));

	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];
		int full = force_full || o->force_full;

		if (!o->image || !o->cur || !o->prev || !o->painted)
			continue;
		/*
		 * EVERY BUFFER IS SPOKEN FOR: one under the raster, one a
		 * flip is waiting on, one already composed and waiting for
		 * the queue. Painting anyway writes into a buffer the kernel
		 * is reading or is about to read. The frame is not lost — the
		 * grid is the session's and the diff below is against what
		 * this screen last painted, so the next visit paints whatever
		 * has accumulated.
		 */
		if (o->back < 0 || o->back >= o->nbuf)
			continue;

		/*
		 * THE COLUMNS THIS SCREEN SHOWS, row by row and a row at a
		 * time. A row past the shared frame's height, or past its
		 * width, is blanked; the overlap is one copy rather than a
		 * bounds test per cell.
		 */
		int keep = w - o->col;

		if (keep < 0)
			keep = 0;
		if (keep > o->cols)
			keep = o->cols;

		for (int y = 0; y < o->rows; y++) {
			KtuiCell *dst = o->cur + (size_t)y * o->cols;

			if (y >= h) {
				memset(dst, 0,
				       (size_t)o->cols * sizeof(*dst));
				continue;
			}
			if (keep)
				memcpy(dst, cur + (size_t)y * w + o->col,
				       (size_t)keep * sizeof(*dst));
			if (keep < o->cols)
				memset(dst + keep, 0,
				       (size_t)(o->cols - keep) * sizeof(*dst));
		}

		/*
		 * THE ARROW COMES OFF BEFORE THE PAINT AND GOES BACK ON AFTER
		 * IT, and the two halves are not symmetrical: the cells under
		 * it are repainted to erase it, and the pixels over them are
		 * rewritten to draw it.
		 *
		 * A MOVE IS A CHANGE THIS SCREEN MUST PRESENT even when not
		 * one cell of it differs — a hand crossing a still desktop is
		 * exactly that — so `moved` carries the frame past the
		 * nothing-changed exit below. Without it the first arrow is
		 * never drawn and every later one is drawn where it was.
		 *
		 * A FULL REPAINT ERASES IT BY ITSELF, so there is nothing to
		 * spoil and nothing owed from the old position.
		 */
		int nx0, ny0, nx1, ny1, ox0, oy0, ox1, oy1;
		int has = ptr_box(o, ptr_x, ptr_y, &nx0, &ny0, &nx1, &ny1);
		int had = full ? 0 : ptr_last_on[i];
		int moved = has != ptr_last_on[i] ||
			    (has && (ptr_x != ptr_last_x[i] ||
				     ptr_y != ptr_last_y[i]));

		if (had && moved &&
		    ptr_box(o, ptr_last_x[i], ptr_last_y[i], &ox0, &oy0, &ox1,
			    &oy1))
			ptr_spoil(o, ox0, oy0, ox1, oy1);

		o->force_full = 0;
		if (!kcell_paint_damage(o->image, o->cur, o->prev, o->cols,
					o->rows, full, 1, o->width, o->height,
					o->painted) && !full && !moved)
			continue;	/* nothing on this screen moved */

		if (has) {
			ptr_draw(o, ptr_x, ptr_y);
			/*
			 * THE ROWS IT COVERS ARE OWED ONLY WHEN IT MOVED. A
			 * row the paint above touched is already in `painted`
			 * and the arrow was redrawn into it; a row it did not
			 * touch holds the same arrow pixels it held last
			 * frame, and `owed` is per buffer and sticky, so every
			 * buffer that has not been given them still owes them
			 * from the frame they were drawn in. Marking them on
			 * every painting frame instead would copy two cell
			 * rows of the mode per frame for nothing.
			 */
			if (moved)
				for (int r = ny0 / ch;
				     r < o->rows && r <= (ny1 - 1) / ch; r++)
					o->painted[r] = 1;
		}
		ptr_last_on[i] = (unsigned char)has;
		ptr_last_x[i] = ptr_x;
		ptr_last_y[i] = ptr_y;

		if (full) {
			owe_all(o);
			/* The strip below the last row is written by the
			 * painter only on a full paint, so this is the one
			 * frame any buffer can be given it from. */
			for (int b = 0; b < o->nbuf; b++)
				o->pad_owed[b] = 1;
		} else
			for (int b = 0; b < o->nbuf; b++) {
				if (!o->owed[b])
					continue;
				for (int y = 0; y < o->rows; y++)
					if (o->painted[y])
						o->owed[b][y] = 1;
			}

		/*
		 * THE ROWS THIS BUFFER HAS NOT BEEN GIVEN, which is not the
		 * same as the rows this frame painted: the buffers are frames
		 * apart, so a row painted last time is still the older frame's
		 * in the two that did not receive it. Copying only what a
		 * buffer owes is what keeps three of them as cheap as one for
		 * a desktop that changes a corner at a time.
		 */
		unsigned char *owed = o->owed[o->back];
		void *bits = o->pixels[o->back];
		/*
		 * THE SHADOW IS THE BOUND, not the dumb buffer. A dumb
		 * buffer's size is page-aligned and the shadow is
		 * stride*height exactly, so a copy sized by the buffer reads
		 * up to a page past the allocation — and the padding it would
		 * carry across is not a pixel any screen shows.
		 */
		size_t lim = (size_t)o->stride * (size_t)o->height;

		if (lim > o->size)
			lim = o->size;
		if (!bits || !owed)
			continue;
		for (int y = 0; y < o->rows;) {
			if (!owed[y]) {
				y++;
				continue;
			}

			int start = y;

			while (y < o->rows && owed[y])
				owed[y++] = 0;

			size_t off = (size_t)start * ch * o->stride;
			size_t n = (size_t)(y - start) * ch * o->stride;

			if (off >= lim)
				break;
			if (off + n > lim)
				n = lim - off;
			memcpy((unsigned char *)bits + off,
			       (const unsigned char *)o->shadow_bits + off, n);
		}
		/*
		 * THE STRIP BELOW THE LAST WHOLE ROW, where the mode is not a
		 * multiple of the cell. No row of `owed` covers it and the
		 * painter fills it only on a full paint, so it carries its own
		 * debt per buffer: given to one buffer alone, it alternates
		 * with whatever the others hold as they come round — a band
		 * across the bottom of the screen blinking at a fraction of
		 * the flip rate, and a slower blink with three buffers than
		 * with two.
		 */
		if (o->pad_owed[o->back]) {
			size_t off = (size_t)o->rows * ch * o->stride;

			o->pad_owed[o->back] = 0;
			if (off < lim)
				memcpy((unsigned char *)bits + off,
				       (const unsigned char *)o->shadow_bits +
					       off, lim - off);
		}

		if (o->nbuf > 1) {
			int e;

			/*
			 * A FLIP IS ALREADY IN FLIGHT, so this frame waits for
			 * the completion to issue its own — see
			 * present_ready(). There is at most one waiting
			 * because `back` goes to -1 the moment there is one on
			 * a three-buffer screen, and presentation order is the
			 * paint order for exactly that reason.
			 */
			if (o->queued >= 0) {
				o->ready = o->back;
				o->back = pick_free(o);
				continue;
			}

			e = do_flip(o, o->back);

			if (e == 0) {
				/*
				 * NO DIRTY RECTANGLE BEHIND A FLIP. A flip
				 * presents the whole buffer, so the rectangle
				 * names nothing the screen has not been given
				 * — and asking is not free. Where .dirty is
				 * `drm_atomic_helper_dirtyfb` it is a
				 * BLOCKING commit that waits on the flip just
				 * queued, so the flush sleeps inside the
				 * ioctl until the flip retires: the frame is
				 * acknowledged a whole scanout late and the
				 * session loses the compose it would have
				 * done in that time. A card that needs the
				 * rectangle to present at all is held to the
				 * single-buffer path by driver_transfers().
				 */
				o->queued = o->back;
				o->flip_pending = 1;
				o->back = pick_free(o);
				continue;
			}

			/*
			 * A REFUSAL IS ONLY PERMANENT WHEN IT IS ABOUT THE
			 * DRIVER. The kernel answers EBUSY for a CRTC whose
			 * framebuffer is detached and EACCES for one that is
			 * not ours this instant — a screen coming back from
			 * blank or from another VT — and neither says the
			 * driver cannot flip. Treating them as proof gives up
			 * every buffer but one for the rest of the session, so
			 * the first screensaver would leave the console
			 * painting into the buffer being scanned out.
			 */
			if (e != EINVAL && e != ENOSYS && e != EOPNOTSUPP) {
				/* The frame stays in the back buffer and the
				 * next one repaints it whole: the CRTC is not
				 * touched, because re-setting it is what
				 * relights a screen the session asked to be
				 * dark. */
				o->force_full = 1;
				continue;
			}

			/*
			 * NO FLIP ON THIS DRIVER, AND THE FRAME IS IN A
			 * BUFFER BEING GIVEN UP. `back` is a buffer nothing is
			 * scanning out, so buffer 0 — the one the single
			 * path keeps — holds an older frame or the zeroes it
			 * was made with: the whole shadow is copied across
			 * before the CRTC is pointed at it, or the modeset
			 * shows a frame that was never painted and a static
			 * screen — a greeter, a prompt — stays that way until
			 * a key is pressed.
			 *
			 * The other buffers are left mapped and unreferenced
			 * rather than freed: out_free() gives every slot back,
			 * and tearing one down here while the CRTC is being
			 * re-pointed is how a screen goes dark for good.
			 */
			o->nbuf = 1;
			o->front = 0;
			o->back = 0;
			o->queued = -1;
			o->ready = -1;
			o->flip_pending = 0;
			if (o->pixels[0] && o->shadow_bits)
				memcpy(o->pixels[0], o->shadow_bits, lim);
			if (o->owed[0])
				memset(o->owed[0], 0, (size_t)o->rows);
			o->pad_owed[0] = 0;
			drmModeSetCrtc(K.drm_fd, o->crtc, o->fb[0], 0, 0,
				       &o->connector, 1, &o->mode);
			/* The whole buffer was rewritten, so the whole buffer
			 * is what a driver that uploads owes the host — the
			 * painted rows are this frame's alone. */
			drmModeClip all = { 0, 0, (unsigned short)o->width,
					    (unsigned short)o->height };

			drmModeDirtyFB(K.drm_fd, o->fb[0], &all, 1);
			continue;
		}

		/* One buffer: whatever was painted is already in the buffer
		 * being scanned out, so all that is left is to say what
		 * changed. */
		kkms_dirty(o, o->fb[0], ch);
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
	.dirty = kkms_owe,
	.poll_event = kkms_poll_event,
	.size = kkms_size,
	.caps = kkms_caps,
	/* THE ONE BACKEND WITH A FRAMEBUFFER OF ITS OWN, so it is the one that
	 * draws a real arrow; everything else keeps libktui's reversed cell. */
	.pointer = kkms_pointer,
	/* THIS BACKEND HOLDS REAL DEVICES, so it answers the raw half too: an
	 * evdev keycode, libinput's own deltas and the compiled layout. A
	 * backend reading a terminal has none of that and leaves both NULL. */
	.poll_raw = kkms_poll_raw,
	.keymap = kkms_keymap_text,
};

int kkms_active(void)
{
	return K.active;
}

int kkms_seat_fd(void)
{
	return K.seat ? libseat_get_fd(K.seat) : -1;
}

int kkms_drm_fd(void)
{
	return K.drm_fd;
}

int kkms_ready(void)
{
	if (!K.active)
		return 0;
	/*
	 * A FREE BUFFER AND NOT AN IDLE FLIP. With three buffers the painter
	 * has one to compose into while a flip is still in flight, which is
	 * the whole of what the third one buys; with two there is none, so
	 * this reduces to "no flip outstanding" exactly as before.
	 *
	 * AN OUTPUT WITH NO BUFFERS AT ALL IS NOT SOMETHING TO WAIT FOR. A
	 * half-built slot answers -1 to pick_free() for the rest of the
	 * session, and a caller that took that for "not yet" would never draw
	 * again on any screen.
	 */
	for (int i = 0; i < K.nout; i++)
		if (K.out[i].nbuf > 0 && K.out[i].back < 0)
			return 0;
	return 1;
}

void kkms_pump(void)
{
	if (K.seat)
		libseat_dispatch(K.seat, 0);
	/*
	 * THE VBLANK, which is what a flip completes on. Reaping it here and
	 * not in the flush is what lets the caller WAIT for it: the descriptor
	 * becomes readable when the screen has taken the frame, so a view that
	 * polls it draws the next one then rather than on a timer.
	 */
	if (K.drm_fd >= 0) {
		drmEventContext ev = {
			.version = 2,
			.page_flip_handler = on_flip,
		};
		struct pollfd p = { .fd = K.drm_fd, .events = POLLIN };

		while (poll(&p, 1, 0) > 0 && (p.revents & POLLIN))
			if (drmHandleEvent(K.drm_fd, &ev) != 0)
				break;
	}
	kkms_input_pump();
}

int kkms_init(const char *seat_name, const char *card, const char *font,
	      const KkmsTune *tune)
{
	memset(&K, 0, sizeof(K));
	K.drm_fd = -1;
	K.drm_dev = -1;
	reason[0] = '\0';

	/*
	 * THE CALLER'S POLICY, COPIED IN BEFORE ANYTHING IS OPENED. It is read
	 * again by every re-probe and every mode change, so it lives in `K`
	 * rather than in the caller's struct — which may be a local that is
	 * gone by the first hotplug — and it is copied AFTER the memset above,
	 * which would otherwise erase it.
	 *
	 * NULL IS EVERY DEFAULT: three buffers, the monitor's preferred mode,
	 * and presentation locked to the vblank. Each of those is the answer
	 * that cannot cost a display.
	 */
	K.want_bufs = tune && tune->buffers ? tune->buffers : KKMS_NBUF;
	if (K.want_bufs < 1 || K.want_bufs > KKMS_NBUF)
		K.want_bufs = KKMS_NBUF;
	K.mode_policy = tune && tune->mode == KKMS_MODE_FASTEST
			? KKMS_MODE_FASTEST : KKMS_MODE_PREFERRED;
	K.async_flip = tune && tune->tearing ? 1 : 0;

	/*
	 * A NAMED SEAT REACHES BOTH HALVES OR NEITHER. libseat reads the
	 * environment and libinput is told directly, so the name is put in
	 * the environment here and read back by kkms_input_init(); passing it
	 * to only one of them lights a screen on one seat and reads input
	 * from another.
	 */
	snprintf(seat, sizeof(seat), "%s", seat_name && *seat_name
					   ? seat_name : "");
	if (seat[0])
		setenv("XDG_SEAT", seat, 1);

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

		if (drmModeSetCrtc(K.drm_fd, o->crtc, front_fb(o), 0, 0,
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

		/*
		 * THE REFRESH, THE BUFFER COUNT AND THE PRESENT, PER SCREEN.
		 * A machine that is slow, that tears, or that came up at 60 on
		 * a 144 Hz panel is a black box otherwise: each of those is a
		 * different answer and the line is the only place they can be
		 * told apart without attaching a debugger to a session that
		 * owns the screen.
		 */
		fprintf(stderr,
			"kdos-view:   output %d: %ux%u@%d.%03dHz, crtc %u, "
			"connector %u, columns %d..%d, %d buffer(s), %s\n",
			i, o->mode.hdisplay, o->mode.vdisplay,
			mode_refresh_mhz(&o->mode) / 1000,
			mode_refresh_mhz(&o->mode) % 1000, o->crtc,
			o->connector, o->col, o->col + o->cols - 1, o->nbuf,
			o->nbuf < 2 ? "dirty rectangle"
			: K.flip_flags ? "unlocked flip (tears)"
			: "flip at vblank");
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
	int rc = 0;

	if (!K.nout)
		return -1;
	snprintf(prev, sizeof(prev), "%s", K.font);

	/*
	 * A LOAD IS THE FONT CHANGE, WHOLE. It replaces the faces, the glyph
	 * cache, the ascii candidate table and the tiling scratch — the
	 * scratch a picture is scaled through is sized in pixels and would
	 * otherwise be reused at the old cell. kcell_font_free() is fcft's own
	 * shutdown and is not refcounted, so calling it here and loading again
	 * tears the library down under a grid that is still being drawn.
	 */
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
	/*
	 * EVERY OUTPUT IS RECUT EVEN IF ONE WILL NOT BE. lay_out() has
	 * already moved every screen's cols and rows, so a slice left cut for
	 * the old cell is a buffer the flush indexes past the end of;
	 * make_slice() leaves the one that failed with no slice, which the
	 * flush skips, and the failure is reported once at the end.
	 */
	for (int i = 0; i < K.nout; i++)
		if (make_slice(&K.out[i]) != 0)
			rc = -1;
	return rc;
}

const char *kkms_font(void)
{
	return K.font;
}

void kkms_shutdown(void)
{
	kkms_input_shutdown();

	/* Every slot, not the first K.nout: a session that lost its last
	 * monitor sits at K.nout = 0 with its buffers still mapped. */
	for (int i = 0; i < KKMS_MAX_OUT; i++)
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
	/* The painter's colour sources and the tiling scratch name a palette
	 * and a cell size that are both about to be gone. */
	kcell_paint_forget();
	kcell_tile_forget();
	ktui_backend_set(NULL);
}

int kkms_switch_vt(int n)
{
	if (!K.seat || n < 1 || n > 63)
		return -1;
	return libseat_switch_session(K.seat, n) == 0 ? 0 : -1;
}

/*
 * EVERY FLIP THE KERNEL HAS NOT YET REPORTED, RETIRED.
 *
 * kkms_flush() skips an output whose flip is outstanding, so a completion
 * that never arrives — the CRTC it was issued against is about to be detached
 * — is that screen never painting again. The wait is bounded and the flag is
 * cleared either way, with the generation moved on so that a completion which
 * does arrive afterwards cannot clear the next flip's flag a frame early.
 */
static void retire_flips(void)
{
	drmEventContext ev = {
		.version = 2,
		.page_flip_handler = on_flip,
	};
	struct pollfd p = { .fd = K.drm_fd, .events = POLLIN };
	int pending = 0;

	if (K.drm_fd < 0)
		return;
	for (int i = 0; i < K.nout; i++)
		pending += K.out[i].flip_pending != 0;

	/* Bounded at a few refreshes: the flag is cleared either way, and a
	 * screen about to go dark has no frame worth waiting for. */
	for (int tries = 0; pending && tries < 3; tries++) {
		if (poll(&p, 1, 34) <= 0 || !(p.revents & POLLIN))
			break;
		if (drmHandleEvent(K.drm_fd, &ev) != 0)
			break;
		pending = 0;
		for (int i = 0; i < K.nout; i++)
			pending += K.out[i].flip_pending != 0;
	}
	for (int i = 0; i < K.nout; i++)
		if (K.out[i].flip_pending || K.out[i].ready >= 0)
			/*
			 * Abandoned, not completed: its event must not name a
			 * buffer as the front one, and the frame composed
			 * behind it is for a screen that is about to go dark.
			 * flip_reset() moves the generation, which is what
			 * makes the completion a stale one.
			 */
			flip_reset(&K.out[i]);
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
 * The state is recorded whatever the session is doing; the ioctls are skipped
 * while it is switched away, because the device is not ours to program then
 * and the VT we switched to has already taken the screen. on_enable() puts
 * the CRTCs back in whichever state the flag names.
 *
 * NOTHING IS PAINTED WHILE THE SCREEN IS DARK. The consumer keeps calling
 * ktui_draw_flush() on its own cadence, and copying, comparing and
 * compositing into a buffer no CRTC is scanning out is a core spent for
 * nothing every tick of a machine that is meant to be asleep. Waking owes the
 * whole screen, because the frames that went past while it was dark were
 * never drawn.
 *
 * GOING DARK RETIRES ANY FLIP IN FLIGHT, because the frame it was waiting for
 * is a frame the detached CRTC will never present.
 */
void kkms_blank(int on)
{
	/*
	 * THE FLAG IS SESSION STATE AND IS SET BEFORE ANY GUARD; only the
	 * ioctls below are device state. The request arrives once — the
	 * consumer tracks its own idea of awake and does not re-send — so a
	 * wake dropped here because the session is switched away would leave
	 * the flag set, and kkms_flush() returns on it: the screen comes back
	 * lit and frozen on the frame it went dark with. on_enable() reads
	 * the flag instead, so the device catches up with it on return.
	 */
	K.blanked = on ? 1 : 0;
	if (K.drm_fd < 0 || !K.nout || !kkms_active())
		return;

	if (on)
		retire_flips();
	else
		/*
		 * WAKING OWES THE WHOLE SCREEN. The frames that went past
		 * while it was dark were dropped before they reached the row
		 * diff, and a full repaint the toolkit asked for during one
		 * of them was cleared with the flag it carried, so nothing
		 * short of a whole frame can be trusted to put the screen
		 * back.
		 */
		for (int i = 0; i < K.nout; i++) {
			K.out[i].force_full = 1;
			owe_all(&K.out[i]);
		}

	/* EVERY screen: one left lit while the rest went dark would be a
	 * machine that looks half asleep. */
	for (int i = 0; i < K.nout; i++) {
		struct kkms_out *o = &K.out[i];

		if (on)
			drmModeSetCrtc(K.drm_fd, o->crtc, 0, 0, 0, NULL, 0,
				       NULL);
		else
			drmModeSetCrtc(K.drm_fd, o->crtc, front_fb(o), 0, 0,
				       &o->connector, 1, &o->mode);
	}
}
