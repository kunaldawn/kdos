/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-display — the screens
 *
 *   ┌ Displays ──────────────────────────────────────────┐
 *   │▸eDP-1      1920x1080@60   on   1.0  normal         │
 *   │ HDMI-A-1   2560x1440@60   off  1.0  normal         │
 *   ├────────────────────────────────────────────────────┤
 *   │ SPACE on/off   m mode   s scale   t rotate  [ ] order│
 *   │ ENTER apply    ESC cancel                          │
 *   └────────────────────────────────────────────────────┘
 *
 * KDOS had NO way to configure a screen. Not a small one, not a text one —
 * none: labwc takes its output configuration over wlr-output-management-v1
 * like every wlroots compositor, no client for it was ported (wlr-randr,
 * kanshi), and nothing in this tree spoke the protocol. A second monitor came
 * up at whatever mode the kernel picked, in whatever position the compositor
 * defaulted to, and a 4K panel stayed at scale 1 with 32-pixel cells on it.
 *
 * THE POSITIONS ARE COMPUTED, NOT TYPED. A protocol that takes pixel
 * coordinates invites a configuration where two screens overlap or one sits in
 * a gap, and neither is a thing anybody wants; what people actually want is an
 * ORDER. So the list order is the left-to-right order, `[` and `]` move a
 * screen along it, and applying lays them out edge to edge from x=0. Logical
 * size, not pixel size: a rotated screen is as wide as it is tall, and a scaled
 * one is smaller than its mode.
 *
 * NOTHING IS APPLIED UNTIL ENTER. The same rule kinstall keeps — every key
 * edits a plan and one action commits it — and it matters more here, because
 * the mistake a display tool makes is a screen you can no longer read.
 *
 * AND AN APPLY THAT SUCCEEDED IS NOT YET KEPT. A mode the monitor cannot show
 * is a `succeeded` from the compositor and an unreadable screen for the user,
 * so success opens a 15-second keep/revert countdown: K keeps (and persists to
 * ~/.config/kdos/displays.conf), R or the timeout re-applies the snapshot
 * taken before ENTER. `kdos-display --apply` is the other half — headless, it
 * replays that file onto the heads it names, which is what a session start or
 * a hotplug runs.
 * ---------------------------------
 */

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <wayland-client.h>

#include "kwl.h"
#include "shell.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"

#define MAX_HEADS 8
#define MAX_MODES 64

struct mode {
	struct zwlr_output_mode_v1 *proxy;
	int32_t w, h, refresh;		/* refresh in mHz */
	int preferred;
};

struct head {
	struct zwlr_output_head_v1 *proxy;
	char name[64];
	char desc[128];
	struct mode modes[MAX_MODES];
	int nmodes;
	int cur_mode;			/* index into modes, or -1 */
	int enabled;
	double scale;
	int32_t transform;
	int32_t x, y;
};

static struct head heads[MAX_HEADS];
static int nheads;
static int order[MAX_HEADS];		/* left to right */
static int sel;

static struct zwlr_output_manager_v1 *mgr;
static uint32_t mgr_serial;
static int have_serial;
static int applied;			/* 1 done, -1 refused */
static char applied_note[128];

/* The keep/revert countdown (F10): a `succeeded` from the compositor proves
 * only that the hardware took the mode, not that the user can read it. */
static double confirm_deadline;		/* monotonic; 0 = no countdown */
static int reverting;			/* the in-flight apply is the revert */
static struct {
	int enabled;
	int cur_mode;
	int32_t transform;
	double scale;
} snap[MAX_HEADS];
static int snap_order[MAX_HEADS];
static int snap_n;			/* heads that existed at snapshot time */

/* The scales offered. Integer-ish and few on purpose: libkwl draws a cell grid
 * and the compositor scales it, so 1.5 is already a compromise and 3 is a
 * screen nobody has. */
static const double SCALES[] = { 1.0, 1.5, 2.0 };
#define NSCALES ((int)(sizeof(SCALES) / sizeof(SCALES[0])))

/* All EIGHT, flips included. A compositor may report any of them, and a table
 * of four labelled a flipped screen `normal` — the one field a display tool
 * exists to show, saying the opposite of the truth — while `t` dropped the flip
 * on the first press. Cycling the whole set keeps a flipped screen flipped as
 * it rotates. */
static const char *const TRANSFORMS[] = {
	"normal", "90", "180", "270",
	"flipped", "flipped-90", "flipped-180", "flipped-270"
};
static const int32_t TRANSFORM_ENUM[] = {
	WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_90,
	WL_OUTPUT_TRANSFORM_180, WL_OUTPUT_TRANSFORM_270,
	WL_OUTPUT_TRANSFORM_FLIPPED, WL_OUTPUT_TRANSFORM_FLIPPED_90,
	WL_OUTPUT_TRANSFORM_FLIPPED_180, WL_OUTPUT_TRANSFORM_FLIPPED_270
};
#define NTRANSFORMS 8

/* ── the mode object ───────────────────────────────────────────────────── */

static struct mode *mode_for(struct head *h, struct zwlr_output_mode_v1 *p)
{
	for (int i = 0; i < h->nmodes; i++)
		if (h->modes[i].proxy == p)
			return &h->modes[i];
	return NULL;
}

static void mode_size(void *data, struct zwlr_output_mode_v1 *p, int32_t w,
		      int32_t h)
{
	struct mode *m = mode_for(data, p);
	if (m) {
		m->w = w;
		m->h = h;
	}
}

static void mode_refresh(void *data, struct zwlr_output_mode_v1 *p, int32_t r)
{
	struct mode *m = mode_for(data, p);
	if (m)
		m->refresh = r;
}

static void mode_preferred(void *data, struct zwlr_output_mode_v1 *p)
{
	struct mode *m = mode_for(data, p);
	if (m)
		m->preferred = 1;
}

static void mode_finished(void *data, struct zwlr_output_mode_v1 *p)
{
	struct mode *m = mode_for(data, p);
	if (m)
		m->proxy = NULL;	/* gone; the row survives until done */
	zwlr_output_mode_v1_destroy(p);
}

static const struct zwlr_output_mode_v1_listener mode_listener = {
	.size = mode_size,
	.refresh = mode_refresh,
	.preferred = mode_preferred,
	.finished = mode_finished,
};

/* ── the head ──────────────────────────────────────────────────────────── */

static void head_name(void *data, struct zwlr_output_head_v1 *p, const char *n)
{
	(void)p;
	snprintf(((struct head *)data)->name, 64, "%s", n);
}

static void head_desc(void *data, struct zwlr_output_head_v1 *p, const char *d)
{
	(void)p;
	snprintf(((struct head *)data)->desc, 128, "%s", d);
}

static void head_phys(void *d, struct zwlr_output_head_v1 *p, int32_t w,
		      int32_t h)
{ (void)d; (void)p; (void)w; (void)h; }

static void head_mode(void *data, struct zwlr_output_head_v1 *p,
		      struct zwlr_output_mode_v1 *m)
{
	struct head *h = data;
	(void)p;
	if (h->nmodes >= MAX_MODES) {
		zwlr_output_mode_v1_destroy(m);
		return;
	}
	struct mode *slot = &h->modes[h->nmodes++];
	memset(slot, 0, sizeof(*slot));
	slot->proxy = m;
	zwlr_output_mode_v1_add_listener(m, &mode_listener, h);
}

static void head_enabled(void *data, struct zwlr_output_head_v1 *p, int32_t e)
{
	(void)p;
	((struct head *)data)->enabled = e != 0;
}

static void head_current_mode(void *data, struct zwlr_output_head_v1 *p,
			      struct zwlr_output_mode_v1 *m)
{
	struct head *h = data;
	(void)p;
	for (int i = 0; i < h->nmodes; i++)
		if (h->modes[i].proxy == m)
			h->cur_mode = i;
}

static void head_position(void *data, struct zwlr_output_head_v1 *p, int32_t x,
			  int32_t y)
{
	struct head *h = data;
	(void)p;
	h->x = x;
	h->y = y;
}

static void head_transform(void *data, struct zwlr_output_head_v1 *p, int32_t t)
{
	(void)p;
	((struct head *)data)->transform = t;
}

static void head_scale(void *data, struct zwlr_output_head_v1 *p,
		       wl_fixed_t scale)
{
	(void)p;
	((struct head *)data)->scale = wl_fixed_to_double(scale);
}

static void head_finished(void *data, struct zwlr_output_head_v1 *p)
{
	struct head *h = data;
	(void)p;
	/*
	 * The screen was unplugged. The row STAYS, with a dead proxy, and is
	 * drawn as gone — it is not compacted out of the array, because every
	 * head listener was registered with a pointer INTO that array and
	 * moving one would leave the compositor writing into a slot that now
	 * belongs to another screen. Nothing can be changed on a dead row and
	 * `apply` skips it.
	 */
	h->proxy = NULL;
	zwlr_output_head_v1_destroy(p);
}

static void head_make(void *d, struct zwlr_output_head_v1 *p, const char *s)
{ (void)d; (void)p; (void)s; }
static void head_model(void *d, struct zwlr_output_head_v1 *p, const char *s)
{ (void)d; (void)p; (void)s; }
static void head_serial(void *d, struct zwlr_output_head_v1 *p, const char *s)
{ (void)d; (void)p; (void)s; }
static void head_adaptive(void *d, struct zwlr_output_head_v1 *p, uint32_t s)
{ (void)d; (void)p; (void)s; }

/*
 * All fourteen, because a listener with a NULL function pointer is a crash the
 * first time the compositor sends that event — make/model/serial arrived in
 * version 2 and adaptive_sync in version 4, and this binds 4.
 */
static const struct zwlr_output_head_v1_listener head_listener = {
	.name = head_name,
	.description = head_desc,
	.physical_size = head_phys,
	.mode = head_mode,
	.enabled = head_enabled,
	.current_mode = head_current_mode,
	.position = head_position,
	.transform = head_transform,
	.scale = head_scale,
	.finished = head_finished,
	.make = head_make,
	.model = head_model,
	.serial_number = head_serial,
	.adaptive_sync = head_adaptive,
};

/* ── the manager ───────────────────────────────────────────────────────── */

static void mgr_head(void *d, struct zwlr_output_manager_v1 *m,
		     struct zwlr_output_head_v1 *p)
{
	(void)d;
	(void)m;
	if (nheads >= MAX_HEADS) {
		zwlr_output_head_v1_destroy(p);
		return;
	}
	struct head *h = &heads[nheads];
	memset(h, 0, sizeof(*h));
	h->proxy = p;
	h->cur_mode = -1;
	h->scale = 1.0;
	order[nheads] = nheads;
	nheads++;
	zwlr_output_head_v1_add_listener(p, &head_listener, h);
}

/*
 * The serial is the whole of the protocol's concurrency control: a
 * configuration is built against one, and if anything changes before it is
 * applied the compositor answers `cancelled` and the serial is dead. Keeping
 * only the newest is therefore not an optimisation, it is the contract.
 */
static void mgr_done(void *d, struct zwlr_output_manager_v1 *m, uint32_t serial)
{
	(void)d;
	(void)m;
	mgr_serial = serial;
	have_serial = 1;
}

static void mgr_finished(void *d, struct zwlr_output_manager_v1 *m)
{
	(void)d;
	zwlr_output_manager_v1_destroy(m);
	mgr = NULL;
}

static const struct zwlr_output_manager_v1_listener mgr_listener = {
	.head = mgr_head,
	.done = mgr_done,
	.finished = mgr_finished,
};

static void reg_global(void *d, struct wl_registry *r, uint32_t name,
		       const char *iface, uint32_t version)
{
	(void)d;
	if (strcmp(iface, zwlr_output_manager_v1_interface.name))
		return;
	uint32_t v = version < 4 ? version : 4;
	mgr = wl_registry_bind(r, name, &zwlr_output_manager_v1_interface, v);
	zwlr_output_manager_v1_add_listener(mgr, &mgr_listener, NULL);
}

static void reg_remove(void *d, struct wl_registry *r, uint32_t n)
{ (void)d; (void)r; (void)n; }

static const struct wl_registry_listener registry_listener = {
	.global = reg_global,
	.global_remove = reg_remove,
};

/* ── applying ──────────────────────────────────────────────────────────── */

static void cfg_succeeded(void *d, struct zwlr_output_configuration_v1 *c)
{
	(void)d;
	zwlr_output_configuration_v1_destroy(c);
	applied = 1;
	snprintf(applied_note, sizeof(applied_note), "applied");
}

static void cfg_failed(void *d, struct zwlr_output_configuration_v1 *c)
{
	(void)d;
	zwlr_output_configuration_v1_destroy(c);
	applied = -1;
	/* The compositor tried it and the hardware said no — a mode the panel
	 * cannot do, or a combination that does not fit the CRTCs. Nothing
	 * changed, which is what `failed` guarantees. */
	snprintf(applied_note, sizeof(applied_note),
		 "the compositor refused that configuration");
}

static void cfg_cancelled(void *d, struct zwlr_output_configuration_v1 *c)
{
	(void)d;
	zwlr_output_configuration_v1_destroy(c);
	applied = -1;
	snprintf(applied_note, sizeof(applied_note),
		 "the screens changed while you were choosing — try again");
}

static const struct zwlr_output_configuration_v1_listener cfg_listener = {
	.succeeded = cfg_succeeded,
	.failed = cfg_failed,
	.cancelled = cfg_cancelled,
};

/* The size a head OCCUPIES once its transform and scale are taken into
 * account, which is what the compositor lays out — not the mode's pixels. */
static void logical_size(const struct head *h, int *w, int *ht)
{
	int mw = 0, mh = 0;
	if (h->cur_mode >= 0 && h->cur_mode < h->nmodes) {
		mw = h->modes[h->cur_mode].w;
		mh = h->modes[h->cur_mode].h;
	}
	int rotated = h->transform == WL_OUTPUT_TRANSFORM_90 ||
		      h->transform == WL_OUTPUT_TRANSFORM_270;
	double s = h->scale > 0.0 ? h->scale : 1.0;
	*w = (int)((rotated ? mh : mw) / s);
	*ht = (int)((rotated ? mw : mh) / s);
}

static void apply_now(void)
{
	if (!mgr || !have_serial) {
		applied = -1;
		snprintf(applied_note, sizeof(applied_note),
			 "this compositor does not offer output management");
		return;
	}

	struct zwlr_output_configuration_v1 *cfg =
		zwlr_output_manager_v1_create_configuration(mgr, mgr_serial);
	if (!cfg) {
		applied = -1;
		return;
	}
	zwlr_output_configuration_v1_add_listener(cfg, &cfg_listener, NULL);

	int x = 0;
	for (int k = 0; k < nheads; k++) {
		struct head *h = &heads[order[k]];
		if (!h->proxy)
			continue;
		if (!h->enabled) {
			zwlr_output_configuration_v1_disable_head(cfg, h->proxy);
			continue;
		}
		struct zwlr_output_configuration_head_v1 *ch =
			zwlr_output_configuration_v1_enable_head(cfg, h->proxy);
		if (!ch)
			continue;
		if (h->cur_mode >= 0 && h->cur_mode < h->nmodes &&
		    h->modes[h->cur_mode].proxy)
			zwlr_output_configuration_head_v1_set_mode(
				ch, h->modes[h->cur_mode].proxy);
		zwlr_output_configuration_head_v1_set_transform(ch, h->transform);
		zwlr_output_configuration_head_v1_set_scale(
			ch, wl_fixed_from_double(h->scale));
		/* Edge to edge from the left, tops aligned. See the header:
		 * the list order IS the arrangement. */
		zwlr_output_configuration_head_v1_set_position(ch, x, 0);
		int lw, lh;
		logical_size(h, &lw, &lh);
		x += lw;
	}
	zwlr_output_configuration_v1_apply(cfg);
}

/* ── keep / revert / persist ───────────────────────────────────────────── */

static double now_s(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

/*
 * The revert baseline, and it is taken ONCE, before any key is pressed.
 * `heads[]` is the plan as well as the compositor's mirror — every edit writes
 * straight into it — so a snapshot taken at ENTER would be the configuration
 * about to be applied, and the countdown would `revert` to exactly the screen
 * the user cannot read.
 */
static void snap_take(void)
{
	for (int i = 0; i < nheads; i++) {
		snap[i].enabled = heads[i].enabled;
		snap[i].cur_mode = heads[i].cur_mode;
		snap[i].transform = heads[i].transform;
		snap[i].scale = heads[i].scale;
	}
	memcpy(snap_order, order, sizeof(order));
	snap_n = nheads;
}

static void snap_restore(void)
{
	/* Only the heads the snapshot saw: one hotplugged mid-countdown has no
	 * saved state and keeps whatever it came up as. */
	for (int i = 0; i < snap_n; i++) {
		heads[i].enabled = snap[i].enabled;
		heads[i].cur_mode = snap[i].cur_mode;
		heads[i].transform = snap[i].transform;
		heads[i].scale = snap[i].scale;
	}
	if (snap_n == nheads)
		memcpy(order, snap_order, sizeof(order));
}

/*
 * ONE RUNG: the fifteen-second countdown. Esc there means revert, which is
 * what the timeout does on its own — so it cannot lose work, and the row can
 * say `Esc revert` instead of promising a close the countdown refuses.
 *
 * The display is the layer's `user` because the revert has to reach the
 * compositor before the frame ends.
 */
static KtuiKeys keys;

static int cd_up(void *user)
{
	(void)user;
	return confirm_deadline > 0.0;
}

static void cd_revert(void *user)
{
	struct wl_display *dpy = user;

	confirm_deadline = 0.0;
	snap_restore();
	reverting = 1;
	apply_now();
	wl_display_flush(dpy);
}

static int conf_path(char *out, size_t n)
{
	const char *home = getenv("HOME");
	if (!home || !*home)
		return -1;
	snprintf(out, n, "%s/.config/kdos/displays.conf", home);
	return 0;
}

/*
 * One line per head, in the format `kdos-display --apply` reads back:
 *   output <name> mode <W>x<H>@<mHz> scale <milli> transform <n> pos <x> <y>
 *   output <name> off
 * A screen switched OFF and kept needs its own row: --apply leaves a head the
 * file does not name exactly as it found it, and at session start that is the
 * enabled state the compositor brought it up in — so recording only the enabled
 * heads made `off` survive until the next login and no further.
 * Written temp+rename: a session killed mid-write must not leave half a file
 * that --apply then replays onto every screen at the next login.
 */
static int persist_layout(void)
{
	char path[512], tmp[520], dir[512];
	const char *home = getenv("HOME");

	if (conf_path(path, sizeof(path)) != 0 || !home)
		return -1;
	snprintf(dir, sizeof(dir), "%s/.config", home);
	mkdir(dir, 0755);
	snprintf(dir, sizeof(dir), "%s/.config/kdos", home);
	mkdir(dir, 0755);

	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE *f = fopen(tmp, "w");
	if (!f)
		return -1;

	int x = 0;
	for (int k = 0; k < nheads; k++) {
		const struct head *h = &heads[order[k]];
		if (!h->proxy)
			continue;
		if (!h->enabled) {
			fprintf(f, "output %s off\n", h->name);
			continue;
		}
		const struct mode *m =
			(h->cur_mode >= 0 && h->cur_mode < h->nmodes)
				? &h->modes[h->cur_mode]
				: NULL;
		int lw, lh;
		logical_size(h, &lw, &lh);
		fprintf(f, "output %s mode %dx%d@%d scale %d transform %d pos %d %d\n",
			h->name, m ? m->w : 0, m ? m->h : 0,
			m ? m->refresh : 0, (int)(h->scale * 1000.0 + 0.5),
			(int)h->transform, x, 0);
		x += lw;
	}
	if (fclose(f) != 0) {
		remove(tmp);
		return -1;
	}
	return rename(tmp, path);
}

/* ── --apply: replay the saved layout, headless ────────────────────────── */

struct saved_head {
	char name[64];
	int off;			/* the head was switched off and kept */
	int w, h, refresh;		/* refresh in mHz; 0x0@0 = auto */
	int scale_milli;
	int transform;
	int x, y;
};

static int load_saved(struct saved_head *out, int max)
{
	char path[512], line[256];
	if (conf_path(path, sizeof(path)) != 0)
		return 0;
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;	/* no config is not an error — nothing to do */
	int n = 0;
	while (n < max && fgets(line, sizeof(line), f)) {
		struct saved_head *c = &out[n];
		char kw[8];
		memset(c, 0, sizeof(*c));
		/* A line that does not parse is skipped, not fatal: an edited
		 * file must not take down every screen at login. */
		if (sscanf(line,
			   "output %63s mode %dx%d@%d scale %d transform %d pos %d %d",
			   c->name, &c->w, &c->h, &c->refresh, &c->scale_milli,
			   &c->transform, &c->x, &c->y) == 8) {
			n++;
		} else if (sscanf(line, "output %63s %7s", c->name, kw) == 2 &&
			   !strcmp(kw, "off")) {
			c->off = 1;
			n++;
		}
	}
	fclose(f);
	return n;
}

static void pump_until_answered(struct wl_display *dpy)
{
	double deadline = now_s() + 3.0;
	int fd = wl_display_get_fd(dpy);

	while (!applied && now_s() < deadline) {
		while (wl_display_prepare_read(dpy) != 0)
			wl_display_dispatch_pending(dpy);
		wl_display_flush(dpy);
		struct pollfd p = { .fd = fd, .events = POLLIN };
		if (poll(&p, 1, 200) > 0)
			wl_display_read_events(dpy);
		else
			wl_display_cancel_read(dpy);
		wl_display_dispatch_pending(dpy);
	}
}

static int apply_saved(struct wl_display *dpy)
{
	struct saved_head saved[MAX_HEADS];
	int nsaved = load_saved(saved, MAX_HEADS);

	if (nsaved == 0 || !mgr || !have_serial)
		return 0;

	struct zwlr_output_configuration_v1 *cfg =
		zwlr_output_manager_v1_create_configuration(mgr, mgr_serial);
	if (!cfg)
		return 0;
	zwlr_output_configuration_v1_add_listener(cfg, &cfg_listener, NULL);

	int touched = 0;
	for (int i = 0; i < nheads; i++) {
		struct head *h = &heads[i];
		if (!h->proxy)
			continue;
		const struct saved_head *c = NULL;
		for (int j = 0; j < nsaved; j++)
			if (!strcmp(saved[j].name, h->name)) {
				c = &saved[j];
				break;
			}
		/*
		 * Omitting a head from a configuration is a protocol error, so
		 * a head the file does not know keeps exactly its current
		 * state rather than being skipped.
		 */
		if (!c) {
			if (!h->enabled) {
				zwlr_output_configuration_v1_disable_head(
					cfg, h->proxy);
				continue;
			}
			struct zwlr_output_configuration_head_v1 *ch =
				zwlr_output_configuration_v1_enable_head(
					cfg, h->proxy);
			if (!ch)
				continue;
			if (h->cur_mode >= 0 && h->cur_mode < h->nmodes &&
			    h->modes[h->cur_mode].proxy)
				zwlr_output_configuration_head_v1_set_mode(
					ch, h->modes[h->cur_mode].proxy);
			zwlr_output_configuration_head_v1_set_transform(
				ch, h->transform);
			zwlr_output_configuration_head_v1_set_scale(
				ch, wl_fixed_from_double(h->scale));
			zwlr_output_configuration_head_v1_set_position(
				ch, h->x, h->y);
			continue;
		}

		touched = 1;
		if (c->off) {
			zwlr_output_configuration_v1_disable_head(cfg, h->proxy);
			continue;
		}
		struct zwlr_output_configuration_head_v1 *ch =
			zwlr_output_configuration_v1_enable_head(cfg, h->proxy);
		if (!ch)
			continue;
		/* Exact mode first; the same size at another refresh second —
		 * a monitor replugged through a different cable loses only the
		 * refresh, not the layout. */
		int mi = -1;
		for (int m = 0; m < h->nmodes && mi < 0; m++)
			if (h->modes[m].proxy && h->modes[m].w == c->w &&
			    h->modes[m].h == c->h &&
			    h->modes[m].refresh == c->refresh)
				mi = m;
		for (int m = 0; m < h->nmodes && mi < 0; m++)
			if (h->modes[m].proxy && h->modes[m].w == c->w &&
			    h->modes[m].h == c->h)
				mi = m;
		if (mi >= 0)
			zwlr_output_configuration_head_v1_set_mode(
				ch, h->modes[mi].proxy);
		zwlr_output_configuration_head_v1_set_transform(
			ch, (c->transform >= 0 && c->transform <= 7)
				    ? c->transform
				    : WL_OUTPUT_TRANSFORM_NORMAL);
		zwlr_output_configuration_head_v1_set_scale(
			ch, wl_fixed_from_double(
				    c->scale_milli > 0
					    ? c->scale_milli / 1000.0
					    : 1.0));
		zwlr_output_configuration_head_v1_set_position(ch, c->x, c->y);
	}

	if (!touched) {
		/* The file names no screen this machine has — apply nothing. */
		zwlr_output_configuration_v1_destroy(cfg);
		return 0;
	}
	zwlr_output_configuration_v1_apply(cfg);
	pump_until_answered(dpy);
	/* A refusal here has nobody watching a screen — this runs at session
	 * start and on hotplug — so it goes to stderr and into the exit status,
	 * or a layout the compositor threw out looks exactly like one it took. */
	if (applied < 0) {
		fprintf(stderr, "kdos-display: %s\n",
			applied_note[0] ? applied_note : "the layout was refused");
		return 1;
	}
	return 0;
}

/*
 * Refusing to switch off the LAST screen is not paternalism: the compositor
 * would have nothing to draw on and this dialog would go with it, leaving a
 * machine that is running and cannot be seen.
 *
 * It is a function rather than a case now because the pointer reaches the same
 * verb through a button, and two copies of this rule is one copy that stops
 * being true.
 */
static void toggle_enabled(struct head *h)
{
	if (!h)
		return;
	if (h->enabled) {
		int live = 0;

		for (int i = 0; i < nheads; i++)
			if (heads[i].enabled)
				live++;
		if (live < 2) {
			snprintf(applied_note, sizeof(applied_note),
				 "that is the only screen there is");
			applied = -1;
			return;
		}
	}
	h->enabled = !h->enabled;
	applied_note[0] = '\0';
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/* Most useful first: the bar drops from the RIGHT, so Close goes before
 * Apply. Escape and a click away still close it. */
enum { DB_APPLY, DB_ONOFF, DB_MODE, DB_SCALE, DB_ROTATE, DB_CLOSE, DB_N };

static void mode_label(const struct head *h, char *out, size_t n)
{
	if (!h->proxy) {
		snprintf(out, n, "%s", "unplugged");
		return;
	}
	if (h->cur_mode < 0 || h->cur_mode >= h->nmodes) {
		snprintf(out, n, "%s", "auto");
		return;
	}
	const struct mode *m = &h->modes[h->cur_mode];
	snprintf(out, n, "%dx%d@%d%s", m->w, m->h, (m->refresh + 500) / 1000,
		 m->preferred ? "*" : "");
}

static void draw(void)
{
	int w = ktui_w, h = ktui_h;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), " Displays ", KT_ACCENT, KT_SURFACE, 0);

	for (int k = 0; k < nheads && k + 2 < h - 3; k++) {
		const struct head *hd = &heads[order[k]];
		char line[160], mode[40];
		int on = k == sel;

		mode_label(hd, mode, sizeof(mode));
		snprintf(line, sizeof(line), "%-12.12s %-14.14s %-4s %.1f  %s",
			 hd->name[0] ? hd->name : "?", mode,
			 hd->enabled ? "on" : "off", hd->scale,
			 TRANSFORMS[(uint32_t)hd->transform < NTRANSFORMS
					    ? hd->transform
					    : 0]);
		ktui_draw_fill(krect(1, 1 + k, w - 2, 1),
			       on ? KT_ACCENT : KT_SURFACE);
		ktui_draw_text(2, 1 + k, w - 4, line,
			       on ? KT_SURFACE
				  : (hd->enabled && hd->proxy ? KT_TEXT
							      : KT_DIM),
			       on ? KT_ACCENT : KT_SURFACE, KT_A_NONE);
	}
	if (!nheads)
		ktui_draw_text(2, 1, w - 4, "no screens reported", KT_MID,
			       KT_SURFACE, KT_A_NONE);

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	/*
	 * EVERY VERB IN THIS WINDOW WAS A KEY, and the row of keys above is a
	 * legend, not a control: a pointer could select a screen and then do
	 * nothing to it — not switch it off, not change its mode, not apply.
	 * The shared bar drops from the right when the window is narrow and
	 * returns where it started, which is where the note has to stop.
	 */
	const struct head *bh = (sel >= 0 && sel < nheads) ? &heads[order[sel]]
							   : NULL;
	int live = 0;
	for (int i = 0; i < nheads; i++)
		if (heads[i].enabled)
			live++;
	struct kch_button b[DB_N];
	b[DB_APPLY] = (struct kch_button){ "Apply", nheads > 0 };
	b[DB_ONOFF] = (struct kch_button){ bh && bh->enabled ? "Turn Off"
							   : "Turn On",
					  bh && bh->proxy &&
						  (!bh->enabled || live > 1) };
	b[DB_MODE] = (struct kch_button){ "Mode", bh && bh->proxy &&
							bh->nmodes > 1 };
	b[DB_SCALE] = (struct kch_button){ "Scale", bh && bh->proxy };
	b[DB_ROTATE] = (struct kch_button){ "Rotate", bh && bh->proxy };
	b[DB_CLOSE] = (struct kch_button){ "Close", 1 };
	int bx = kch_buttons(w, h - 2, b, DB_N, -1);
	int room = bx - 3;
	/* A MESSAGE takes whatever room is left. The keys are the row's now,
	 * one row up, where the legend used to be written by hand. */
	if (applied_note[0] && room > 0)
		ktui_draw_text(2, h - 2, room, applied_note,
			       applied < 0 ? KT_ERR : KT_DIM, KT_SURFACE,
			       KT_A_NONE);

	/*
	 * `s` SCALE AND `t` ROTATE ARE DELIBERATELY NOT PUSHED. This window is
	 * sixty columns and eight hints need seventy-four; the row stops at
	 * the first that does not fit, and what it would drop is the Esc hint
	 * that must always be there. The Scale and Rotate buttons name those
	 * two actions on the bar. Widening the window is the fix if they are
	 * wanted here — never reordering Esc.
	 */
	ktui_hint_if(confirm_deadline > 0.0, "K", "keep");
	ktui_hint_if(confirm_deadline > 0.0, "R", "revert");
	/* The bar is drawn with focus -1 and so pushes no Enter hint of its
	 * own; this surface names its own Apply. */
	ktui_hint_if(confirm_deadline == 0.0 && nheads > 0, "Enter", "apply");
	ktui_hint_if(confirm_deadline == 0.0 && bh && bh->proxy &&
			     (!bh->enabled || live > 1),
		     "Space", "on/off");
	ktui_hint_if(confirm_deadline == 0.0 && bh && bh->proxy &&
			     bh->nmodes > 1,
		     "m", "mode");
	ktui_hint_if(confirm_deadline == 0.0 && nheads > 1, "[/]", "order");
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 3, w - 4, 1), KT_SURFACE);
	ktui_draw_flush();
}

/* ── the plan, as keys ─────────────────────────────────────────────────── */

static int preferred_mode(const struct head *h)
{
	for (int i = 0; i < h->nmodes; i++)
		if (h->modes[i].preferred)
			return i;
	return 0;
}

static void cycle_mode(struct head *h, int dir)
{
	if (h->nmodes < 1)
		return;
	/* From `auto`, the first press lands on the preferred mode — the one
	 * the monitor itself advertises — not on whatever slot came first. */
	if (h->cur_mode < 0) {
		h->cur_mode = preferred_mode(h);
		return;
	}
	h->cur_mode = (h->cur_mode + dir + h->nmodes) % h->nmodes;
}

static void cycle_scale(struct head *h)
{
	int i = 0;
	for (; i < NSCALES; i++)
		if (h->scale > SCALES[i] - 0.01 && h->scale < SCALES[i] + 0.01)
			break;
	h->scale = SCALES[(i + 1) % NSCALES];
}

static void cycle_transform(struct head *h)
{
	int i = 0;
	for (; i < NTRANSFORMS; i++)
		if (h->transform == TRANSFORM_ENUM[i])
			break;
	h->transform = TRANSFORM_ENUM[(i + 1) % NTRANSFORMS];
}

static void move_in_order(int dir)
{
	int j = sel + dir;
	if (sel < 0 || sel >= nheads || j < 0 || j >= nheads)
		return;
	int t = order[sel];
	order[sel] = order[j];
	order[j] = t;
	sel = j;
}

/* ── main ──────────────────────────────────────────────────────────────── */

static void print_list(void)
{
	for (int k = 0; k < nheads; k++) {
		const struct head *h = &heads[order[k]];
		char mode[40];
		int lw, lh;
		mode_label(h, mode, sizeof(mode));
		logical_size(h, &lw, &lh);
		printf("%s\t%s\t%s\tscale=%.2f\ttransform=%s\tat=%d,%d\tlogical=%dx%d\t%s\n",
		       h->name, mode, h->enabled ? "on" : "off", h->scale,
		       TRANSFORMS[(uint32_t)h->transform < NTRANSFORMS
					  ? h->transform : 0],
		       h->x, h->y, lw, lh, h->desc);
	}
}

int display_main(int argc, char **argv)
{
	const char *font = NULL;
	int list_only = 0, apply_only = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--list"))
			list_only = 1;
		else if (!strcmp(argv[i], "--apply"))
			apply_only = 1;
		else {
			fprintf(stderr, "usage: kdos-display [--list] "
					"[--apply] [--font NAME]\n");
			return 2;
		}
	}

	/*
	 * --list and --apply need the protocol and no surface, which is the
	 * same shape `kdos-shell --dump` uses and for the same reason: the
	 * answer is worth having in a bug report without a screen to draw it
	 * on — and --apply runs at session start and on hotplug, where there
	 * is nobody to draw for.
	 */
	KDispConfig cfg = {
		.role = (list_only || apply_only) ? KDISP_ROLE_NONE
						  : KDISP_ROLE_OVERLAY,
		.cols = 60,
		.rows = 14,
		.app_id = "kdos-display",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0) {
		fprintf(stderr, "kdos-display: no compositor\n");
		return 1;
	}

	/*
	 * NULL ON THE CONSOLE, and this is the guard that stops it being a
	 * crash. `kdisp_init` succeeds there — the console backend probes
	 * first and takes it — so reaching past libkdisp for a Wayland
	 * display hands `wl_display_get_registry` a null pointer, from a Start
	 * menu row a person can click.
	 *
	 * Output management is Wayland's. The console desktop has one grid at
	 * one size, so there is nothing here for this program to arrange.
	 */
	struct wl_display *dpy = kwl_display();

	if (!dpy) {
		fprintf(stderr, "kdos-display: the console desktop has one "
				"screen and no output management\n");
		kdisp_shutdown();
		return 1;
	}

	struct wl_registry *reg = wl_display_get_registry(dpy);

	wl_registry_add_listener(reg, &registry_listener, NULL);
	wl_display_roundtrip(dpy);		/* the manager */
	wl_display_roundtrip(dpy);		/* its heads */
	wl_display_roundtrip(dpy);		/* and their modes */

	if (!mgr) {
		fprintf(stderr, "kdos-display: this compositor has no "
				"wlr-output-management\n");
		kdisp_shutdown();
		return 1;
	}

	if (list_only) {
		print_list();
		kdisp_shutdown();
		return 0;
	}
	if (apply_only) {
		int rc = apply_saved(dpy);
		kdisp_shutdown();
		return rc;
	}

	snap_take();
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_SURFACE);
	/* The page in /usr/share/kdos/doc that F1 opens. A name with no
	 * file there is refused by testing/preflight.sh. */
	keys.doc = "display";
	keys.help = sh_help;
	ktui_keys_layer(&keys, "revert", cd_up, cd_revert, dpy);

	while (!kdisp_should_close()) {
		/* Follow a live `kdos theme <accent>`; see sh_theme_poll(). */
		sh_theme_poll();
		/*
		 * An apply that succeeded does NOT close the dialog: the mode
		 * may be one the monitor cannot show, so the answer is a
		 * countdown the user must interrupt to keep it. A revert's
		 * success just says so.
		 */
		if (applied > 0) {
			applied = 0;
			if (reverting) {
				reverting = 0;
				snprintf(applied_note, sizeof(applied_note),
					 "reverted");
			} else if (confirm_deadline == 0.0) {
				confirm_deadline = now_s() + 15.0;
			}
		}
		if (applied < 0)
			reverting = 0;
		if (confirm_deadline > 0.0) {
			int left = (int)(confirm_deadline - now_s() + 0.999);
			if (left <= 0) {
				confirm_deadline = 0.0;
				snap_restore();
				reverting = 1;
				apply_now();
				wl_display_flush(dpy);
			} else {
				snprintf(applied_note, sizeof(applied_note),
					 "Keep these settings? reverting in %ds — press K",
					 left);
			}
		}

		draw();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 500)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		{
			int r = ktui_keys(&keys, &ev);

			if (r == KTUI_KEY_CLOSE)
				goto done;
			if (r == KTUI_KEY_TAKEN)
				continue;
		}

		/* The countdown answers two keys and nothing else: editing a
		 * plan mid-revert would apply a state nobody chose. */
		if (confirm_deadline > 0.0) {
			if (ev.type == KT_EVT_KEY &&
			    (ev.key == 'k' || ev.key == 'K')) {
				confirm_deadline = 0.0;
				persist_layout();
				goto done;
			}
			if (ev.type == KT_EVT_KEY &&
			    (ev.key == 'r' || ev.key == 'R')) {
				confirm_deadline = 0.0;
				snap_restore();
				reverting = 1;
				apply_now();
				wl_display_flush(dpy);
			}
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG) {
				/* The button bar lights under the pointer. The
				 * LIST does not follow the pointer here: a row
				 * is a screen and the buttons act on the
				 * selected one, so hover-to-select would move
				 * the target out from under the hand between
				 * aiming and clicking. */
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press != KT_MP_PRESS || ev.mx < 0)
				continue;
			if (ev.btn == KT_MB_RIGHT)
				break;
			if (ev.btn == KT_MB_WHEEL_UP) {
				if (sel > 0)
					sel--;
				continue;
			}
			if (ev.btn == KT_MB_WHEEL_DOWN) {
				if (sel + 1 < nheads)
					sel++;
				continue;
			}
			if (ev.btn != KT_MB_LEFT)
				continue;
			int row = ev.my - 1;
			int bi = kch_button_at(ev.mx, ev.my);
			if (bi >= 0) {
				struct head *hh = (sel >= 0 && sel < nheads)
							  ? &heads[order[sel]]
							  : NULL;
				if (hh && !hh->proxy)
					hh = NULL;
				switch (bi) {
				case DB_APPLY:
					reverting = 0;
					applied = 0;
					applied_note[0] = '\0';
					apply_now();
					wl_display_flush(dpy);
					break;
				case DB_ONOFF:
					toggle_enabled(hh);
					break;
				case DB_MODE:
					if (hh)
						cycle_mode(hh, 1);
					break;
				case DB_SCALE:
					if (hh)
						cycle_scale(hh);
					break;
				case DB_ROTATE:
					if (hh)
						cycle_transform(hh);
					break;
				case DB_CLOSE:
					goto done;
				}
				continue;
			}
			if (row >= 0 && row < nheads)
				sel = row;
			continue;
		}
		if (ev.type != KT_EVT_KEY)
			continue;

		struct head *h = (sel >= 0 && sel < nheads)
					 ? &heads[order[sel]]
					 : NULL;
		/* Nothing is editable on a screen that has been unplugged. */
		if (h && !h->proxy)
			h = NULL;
		switch (ev.key) {
		case KT_K_UP:
			if (sel > 0)
				sel--;
			break;
		case KT_K_DOWN:
			if (sel + 1 < nheads)
				sel++;
			break;
		case ' ':
			toggle_enabled(h);
			break;
		case 'm':
			if (h)
				cycle_mode(h, 1);
			break;
		case 'M':
			if (h)
				cycle_mode(h, -1);
			break;
		case 's':
			if (h)
				cycle_scale(h);
			break;
		case 't':
			if (h)
				cycle_transform(h);
			break;
		case '[':
			move_in_order(-1);
			break;
		case ']':
			move_in_order(1);
			break;
		case KT_K_ENTER:
			/* The countdown's revert replays the baseline
			 * snap_take() recorded before the first edit. */
			reverting = 0;
			applied = 0;
			applied_note[0] = '\0';
			apply_now();
			wl_display_flush(dpy);
			break;
		default:
			break;
		}
	}
done:
	kdisp_shutdown();
	return applied < 0 ? 1 : 0;
}
