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

/* The buffer the CRTC is showing: the one that is not being painted. */
static uint32_t front_fb(const struct kkms_out *o);
/* Every row of this output is a frame behind in both buffers. */
static void owe_all(struct kkms_out *o);

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
		/* A flip the kernel will never report now: the device was
		 * somebody else's while we were away. */
		o->flip_pending = 0;
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
 * buffer there tears exactly like hardware and the pair is what removes it.
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
			K.drm_fd = fd;
			K.drm_dev = id;
			K.res = res;
			K.drm_transfers = driver_transfers(fd);
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

		/* Unless this screen was already set to one it still
		 * publishes, which is a decision and outranks the default. */
		for (int k = 0; k < nwas; k++) {
			if (!was[k].have || was[k].conn != c->connector_id)
				continue;
			for (int m = 0; m < c->count_modes; m++)
				if (c->modes[m].hdisplay ==
					    was[k].mode.hdisplay &&
				    c->modes[m].vdisplay ==
					    was[k].mode.vdisplay &&
				    c->modes[m].vrefresh ==
					    was[k].mode.vrefresh) {
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
 * A SCANOUT BUFFER. One of a pair, plus the painter's own in system memory.
 *
 * The pair is what makes a flip possible, and a flip is what makes an
 * animation tear-free: a single buffer is rewritten row by row while the
 * raster is inside it, and a row caught mid-paint shows the old glyph above
 * the new one. The cost of the pair is a copy of the rows that changed, which
 * on a cell grid is what changed and nothing more.
 *
 * The painter never touches either of them. A dumb buffer is mapped
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
 * second buffer: make_buf() fills the handle and the framebuffer id before it
 * can fail at the mapping, and a slot merely zeroed leaves a whole screen of
 * GPU memory that nothing can reach again, because the fields are the only
 * record of it.
 *
 * `stride` and `size` describe BOTH slots and are left alone: the other
 * buffer is still mapped with them.
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

static int make_fb(struct kkms_out *o)
{
	if (make_buf(o, 0) != 0)
		return -1;
	/*
	 * A SECOND BUFFER IS WANTED, NOT REQUIRED, AND NOT ALWAYS WANTED.
	 *
	 * A driver with no memory for one, and a machine with several large
	 * screens, still get a desktop: `nbuf` says which of the two presents
	 * is in force. A transfer-model driver is not even asked — there a
	 * flip costs the whole plane and buys nothing, because the host never
	 * reads the buffer except at the copy the dirty rectangle triggers.
	 */
	o->nbuf = !K.drm_transfers && make_buf(o, 1) == 0 ? 2 : 1;
	if (o->nbuf == 1) {
		reason[0] = '\0';	/* not a failure, and not reported as one */
		buf_free(o, 1);
	}
	o->back = o->nbuf - 1;
	o->flip_pending = 0;
	/* New buffers are a new flip identity: a completion still in the
	 * kernel's queue names the pair that is gone. */
	o->flip_gen++;
	o->pad_owed[0] = o->pad_owed[1] = 0;
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

static uint32_t front_fb(const struct kkms_out *o)
{
	return o->fb[o->nbuf > 1 ? !o->back : 0];
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
 * THE FLIP COMPLETED. Nothing is done with the frame it showed: the buffer it
 * replaced is now the one to paint into, and `owed` already says which of its
 * rows are a frame behind.
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
	o->flip_pending = 0;
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
	for (int i = 0; i < 2; i++) {
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
	free(o->owed[0]);
	free(o->owed[1]);
	o->painted = calloc((size_t)o->rows, 1);
	o->owed[0] = calloc((size_t)o->rows, 1);
	o->owed[1] = calloc((size_t)o->rows, 1);
	if (!o->painted || !o->owed[0] || !o->owed[1]) {
		slice_free(o);
		return fail_with("the output's grid", "out of memory");
	}
	o->force_full = 1;
	/* A different number of rows puts the strip below them somewhere
	 * else, so both buffers owe it wherever they are in the pair. */
	o->pad_owed[0] = o->pad_owed[1] = 1;
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
	for (int i = 0; i < 2; i++)
		buf_free(o, i);
	slice_free(o);
	o->nbuf = 0;
	o->back = 0;
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
			 * REPORTED. kkms_flush() skips an output with a flip
			 * outstanding, so a flag left set here freezes this
			 * screen for the rest of the session — and the
			 * generation moves with it, or the cancelled flip's
			 * completion clears the NEXT one a frame early.
			 */
			o->flip_pending = 0;
			o->flip_gen++;
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
	/*
	 * THE TIMINGS ARE NOT THE FRAME RATE ON THEIR OWN. Interlace sends two
	 * fields per frame, doublescan sends each line twice and vscan sends
	 * each line n times; the same three corrections the kernel applies in
	 * drm_mode_vrefresh, in the same order, or 1080i is offered as 30 Hz
	 * beside the 60 Hz every other tool reports.
	 */
	if (d->flags & DRM_MODE_FLAG_INTERLACE)
		m->refresh *= 2;
	if (d->flags & DRM_MODE_FLAG_DBLSCAN)
		m->refresh /= 2;
	if (d->vscan > 1)
		m->refresh /= d->vscan;
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

/* Every row of this output is a frame behind in both buffers: a full repaint,
 * a mode change, or coming back from another VT. */
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
 * writes into the buffer the screen is reading: the painter draws into the
 * shadow, the rows it touched are copied into the back buffer, and the CRTC
 * is pointed at that buffer at the next vblank. A row caught by the raster
 * mid-paint is what tearing IS, and there is no longer a moment when one
 * could be.
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
		/* A flip this backend asked for has not completed: the back
		 * buffer is still the one on the screen. */
		if (o->flip_pending)
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

		o->force_full = 0;
		if (!kcell_paint_damage(o->image, o->cur, o->prev, o->cols,
					o->rows, full, 1, o->width, o->height,
					o->painted) && !full)
			continue;	/* nothing on this screen moved */

		if (full) {
			owe_all(o);
			/* The strip below the last row is written by the
			 * painter only on a full paint, so this is the one
			 * frame either buffer can be given it from. */
			o->pad_owed[0] = o->pad_owed[1] = 1;
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
		 * same as the rows this frame painted: the two buffers are a
		 * frame apart, so one that was painted last time is still the
		 * older frame's here. Copying only what it owes is what keeps
		 * a pair as cheap as a single buffer for a desktop that
		 * changes a corner at a time.
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
		 * with whatever the other holds on every flip — a band across
		 * the bottom of the screen blinking at half the flip rate.
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
			int rc = drmModePageFlip(K.drm_fd, o->crtc,
						 o->fb[o->back],
						 DRM_MODE_PAGE_FLIP_EVENT,
						 flip_cookie(o));

			if (rc == 0) {
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
				o->flip_pending = 1;
				o->back = !o->back;
				continue;
			}

			/*
			 * A REFUSAL IS ONLY PERMANENT WHEN IT IS ABOUT THE
			 * DRIVER. The kernel answers EBUSY for a CRTC whose
			 * framebuffer is detached and EACCES for one that is
			 * not ours this instant — a screen coming back from
			 * blank or from another VT — and neither says the
			 * driver cannot flip. Treating them as proof gives up
			 * the pair for the rest of the session, so the first
			 * screensaver would leave the console painting into
			 * the buffer being scanned out.
			 *
			 * libdrm's ioctl wrapper returns -errno on some paths
			 * and -1 on others, so the sign is not relied on.
			 */
			int e = rc < 0 && rc != -1 ? -rc : errno;

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
			 * NO FLIP ON THIS DRIVER, AND THE FRAME IS IN THE
			 * BUFFER BEING GIVEN UP. `back` is the buffer nothing
			 * is scanning out, so the surviving one holds an
			 * older frame or the zeroes it was made with: the
			 * whole shadow is copied across before the CRTC is
			 * pointed at it, or the modeset shows a frame that
			 * was never painted and a static screen — a greeter,
			 * a prompt — stays that way until a key is pressed.
			 */
			o->nbuf = 1;
			o->back = 0;
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
	for (int i = 0; i < K.nout; i++)
		if (K.out[i].flip_pending)
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

int kkms_init(const char *seat_name, const char *card, const char *font)
{
	memset(&K, 0, sizeof(K));
	K.drm_fd = -1;
	K.drm_dev = -1;
	reason[0] = '\0';

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
		if (K.out[i].flip_pending) {
			K.out[i].flip_pending = 0;
			/* Abandoned, not completed: its event must not clear
			 * the flag belonging to the next flip. */
			K.out[i].flip_gen++;
		}
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
