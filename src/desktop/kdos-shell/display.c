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
 * ---------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* The scales offered. Integer-ish and few on purpose: libkwl draws a cell grid
 * and the compositor scales it, so 1.5 is already a compromise and 3 is a
 * screen nobody has. */
static const double SCALES[] = { 1.0, 1.5, 2.0 };
#define NSCALES ((int)(sizeof(SCALES) / sizeof(SCALES[0])))

static const char *const TRANSFORMS[] = { "normal", "90", "180", "270" };
static const int32_t TRANSFORM_ENUM[] = {
	WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_90,
	WL_OUTPUT_TRANSFORM_180, WL_OUTPUT_TRANSFORM_270
};
#define NTRANSFORMS 4

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

/* ── drawing ───────────────────────────────────────────────────────────── */

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
	snprintf(out, n, "%dx%d@%d", m->w, m->h, (m->refresh + 500) / 1000);
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
		ktui_draw_text(2, 1, w - 4, "no screens reported", KT_DIM,
			       KT_SURFACE, KT_A_NONE);

	ktui_draw_hline(1, h - 4, w - 2, KT_G_HL, KT_DIM, KT_SURFACE);
	ktui_draw_text(2, h - 3, w - 4,
		       "SPACE on/off   m mode   s scale   t rotate   [ ] order",
		       KT_DIM, KT_SURFACE, KT_A_NONE);
	ktui_draw_text(2, h - 2, w - 4,
		       applied_note[0] ? applied_note
				       : "ENTER apply    ESC cancel",
		       applied < 0 ? KT_ERR : KT_DIM, KT_SURFACE, KT_A_NONE);
	ktui_draw_flush();
}

/* ── the plan, as keys ─────────────────────────────────────────────────── */

static void cycle_mode(struct head *h)
{
	if (h->nmodes < 1)
		return;
	h->cur_mode = (h->cur_mode + 1) % h->nmodes;
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
	int list_only = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--list"))
			list_only = 1;
		else {
			fprintf(stderr, "usage: kdos-display [--list] "
					"[--font NAME]\n");
			return 2;
		}
	}

	/*
	 * --list needs the protocol and no surface, which is the same shape
	 * `kdos-shell --dump` uses and for the same reason: the answer is worth
	 * having in a bug report without a screen to draw it on.
	 */
	KwlConfig cfg = {
		.role = list_only ? KWL_ROLE_NONE : KWL_ROLE_OVERLAY,
		.cols = 60,
		.rows = 14,
		.app_id = "kdos-display",
		.font = font,
		.keyboard = 1,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-display: no compositor\n");
		return 1;
	}

	struct wl_display *dpy = kwl_display();
	struct wl_registry *reg = wl_display_get_registry(dpy);
	wl_registry_add_listener(reg, &registry_listener, NULL);
	wl_display_roundtrip(dpy);		/* the manager */
	wl_display_roundtrip(dpy);		/* its heads */
	wl_display_roundtrip(dpy);		/* and their modes */

	if (!mgr) {
		fprintf(stderr, "kdos-display: this compositor has no "
				"wlr-output-management\n");
		kwl_shutdown();
		return 1;
	}

	if (list_only) {
		print_list();
		kwl_shutdown();
		return 0;
	}

	ktui_draw_init();

	while (!kwl_should_close()) {
		draw();

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, 500)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			/* An apply that succeeded closes the dialog: the answer
			 * is on the screens themselves. */
			if (applied > 0)
				break;
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			if (ev.press != KT_MP_PRESS || ev.mx < 0)
				continue;
			if (ev.btn == KT_MB_RIGHT)
				break;
			int row = ev.my - 1;
			if (ev.btn == KT_MB_LEFT && row >= 0 && row < nheads)
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
		case KT_K_ESC:
			goto done;
		case KT_K_UP:
			if (sel > 0)
				sel--;
			break;
		case KT_K_DOWN:
			if (sel + 1 < nheads)
				sel++;
			break;
		case ' ':
			/*
			 * Refusing to switch off the LAST screen is not
			 * paternalism: the compositor would have nothing to draw
			 * on and this dialog would go with it, leaving a machine
			 * that is running and cannot be seen.
			 */
			if (!h)
				break;
			if (h->enabled) {
				int live = 0;
				for (int i = 0; i < nheads; i++)
					if (heads[i].enabled)
						live++;
				if (live < 2) {
					snprintf(applied_note,
						 sizeof(applied_note),
						 "that is the only screen there is");
					applied = -1;
					break;
				}
			}
			h->enabled = !h->enabled;
			applied_note[0] = '\0';
			break;
		case 'm':
			if (h)
				cycle_mode(h);
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
	kwl_shutdown();
	return applied < 0 ? 1 : 0;
}
