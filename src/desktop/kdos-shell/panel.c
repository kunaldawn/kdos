/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-shell — the panel
 *
 *   ╔═══════════════════════════════════════════════════════════════════╗
 *   ║ ▶ KDOS │ 1 2 ▓3▓ 4 │ foot  firefox-esr  gimp │ ●MIC firefox  K N  41% 21:07 ║
 *   ╚═══════════════════════════════════════════════════════════════════╝
 *
 * One row of character cells, drawn with libktui through libkwl into a
 * layer-shell surface. The same widgets kinstall and kdosbuild use, on the
 * same eight colour slots, in the same palette — a panel that renders as a
 * grid rather than as a picture is the whole visual identity of this desktop,
 * and it is why the panel and the boot splash and the tty look like one
 * machine.
 *
 * Everything it knows it learns from STANDARD protocols: the window list from
 * wlr-foreign-toplevel-management, the workspaces from ext-workspace-v1. There
 * is no private channel to kdos-comp, which is what lets someone run waybar
 * here instead if they want to.
 *
 * The right wing reads /sys and /proc directly. No upower, no D-Bus for
 * battery — those are three daemons and an IPC round trip to read a file that
 * the kernel already exports as text. The two things it cannot read from a file
 * are the tray (pure D-Bus, tray.c) and which app is recording (the PipeWire
 * graph, privacy.c).
 */

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

/* ── the right wing: clock, battery, and what else is measurable ───────── */

/*
 * Battery, from /sys/class/power_supply.
 *
 * Reported as `capacity` if the kernel gives one and computed from
 * charge_now/charge_full otherwise — laptops disagree about which they export,
 * and a panel that shows nothing on half of them is worse than one that does
 * the division.
 */
static int battery_percent(int *charging)
{
	DIR *d = opendir("/sys/class/power_supply");
	struct dirent *e;
	int pct = -1;

	*charging = 0;
	if (!d)
		return -1;
	while ((e = readdir(d))) {
		char path[512], buf[64];
		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type",
			 e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) != 0)
			continue;
		if (strcmp(buf, "Battery"))
			continue;

		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/capacity", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0) {
			pct = atoi(buf);
		} else {
			long now = 0, full = 0;
			snprintf(path, sizeof(path),
				 "/sys/class/power_supply/%s/charge_now", e->d_name);
			if (sh_read_line(path, buf, sizeof(buf)) == 0)
				now = atol(buf);
			snprintf(path, sizeof(path),
				 "/sys/class/power_supply/%s/charge_full", e->d_name);
			if (sh_read_line(path, buf, sizeof(buf)) == 0)
				full = atol(buf);
			if (full > 0)
				pct = (int)(now * 100 / full);
		}

		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/status", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0)
			*charging = !strcmp(buf, "Charging");
		break;
	}
	closedir(d);
	return pct;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * The panel is drawn RIGHT TO LEFT after the left wing, because the right wing
 * has a fixed width and the window list is what has to give when the screen is
 * narrow. Laying out left to right and hoping instead is how a clock ends up
 * pushed off the edge of a 1280-wide display by one long window title.
 */
static void draw_panel(struct sh_state *sh)
{
	int w = ktui_w, h = ktui_h;
	if (w < 20 || h < 1)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);

	/* ── left: the menu bar, then the workspaces ──
	 *
	 * GNOME 2's three words, behind the ≡ mark that stands in for the
	 * distributor logo an icon theme would have supplied. The mark is a
	 * button too — it opens Applications, which is what a person aiming at
	 * a logo in the corner of a screen expects.
	 */
	int x = 1;
	sh->menu_hit_x[0] = x;
	x += ktui_draw_text(x, 0, w - x, SH_MENU_MARK, KT_ACCENT, KT_SURFACE,
			    KT_A_NONE);
	x += 1;
	for (int i = 0; i < SH_NMENUS; i++) {
		sh->menu_hit_x[i] = x;
		x += ktui_draw_text(x, 0, w - x, sh_menu_labels[i],
				    i == sh->menu_open ? KT_SURFACE : KT_TEXT,
				    i == sh->menu_open ? KT_ACCENT : KT_SURFACE,
				    KT_A_NONE);
		sh->menu_hit_end[i] = x;
		x += 2;
	}
	x += ktui_draw_text(x, 0, w - x, ktui_glyph[KT_G_VL], KT_DIM,
			    KT_SURFACE, KT_A_NONE);
	x += 1;

	/*
	 * ext-workspace-v1 has no "there are windows here" state — ACTIVE,
	 * URGENT and HIDDEN are all it carries — so occupancy is DERIVED: the
	 * workspace being shown is occupied exactly when a window is not
	 * minimized, since kdos-comp reports a window on another workspace as
	 * MINIMIZED. That is right for every workspace the user has visited and
	 * silent about the rest, which is the honest shape: it never claims a
	 * workspace is empty on the strength of a state the protocol does not
	 * have. Reading URGENT alone, as this did, meant NO workspace was ever
	 * drawn as occupied.
	 */
	if (sh->active_ws >= 0 && sh->active_ws < SH_MAX_WS) {
		int live = 0;
		for (int i = 0; i < sh->ntasks; i++)
			if (!sh->tasks[i].minimized)
				live = 1;
		sh->ws_occupied[sh->active_ws] = live;
	}

	sh->ws_hit_x = x;
	for (int i = 0; i < sh->nws && x < w - 4; i++) {
		char label[8];
		snprintf(label, sizeof(label), "%d", i + 1);
		/*
		 * The active workspace is drawn REVERSED — the one emphasis
		 * that reads identically on a tty and in a truecolor surface.
		 * The slots are swapped for it, so the ATTRIBUTE must not be
		 * set as well: passing both swaps them back, and the active
		 * workspace came out looking exactly like an inactive one that
		 * happened to be occupied.
		 */
		int active = i == sh->active_ws;
		int fg = sh->ws_occupied[i] ? KT_ACCENT : KT_DIM;
		x += ktui_draw_text(x, 0, w - x, label, active ? KT_SURFACE : fg,
				    active ? KT_ACCENT : KT_SURFACE,
				    KT_A_NONE);
		x += 1;
	}
	sh->ws_hit_end = x;

	x += ktui_draw_text(x, 0, w - x, ktui_glyph[KT_G_VL], KT_DIM,
			    KT_SURFACE, KT_A_NONE);
	x += 1;

	/* ── right: clock first, since it is the one thing that must not move ── */
	char right[64];
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);

	/*
	 * Checked once a minute, not once a frame: `kdos restarts` walks every
	 * process's maps, and the answer only changes when a package is
	 * installed.
	 */
	static time_t last_restart_check;
	static int restarts;
	if (now - last_restart_check > 60) {
		last_restart_check = now;
		restarts = sh_restart_count();
	}

	int charging = 0, pct = battery_percent(&charging);
	/* A leading mark rather than a word: the panel is one row, and the point
	 * is to be noticed and clicked, not to explain itself in situ. Built in
	 * one format rather than prepended afterwards — the compiler is right
	 * that a two-step build can overflow the field. */
	const char *mark = restarts > 0 ? ktui_glyph[KT_G_BULLET] : "";
	const char *gap = restarts > 0 ? "  " : "";
	if (pct >= 0)
		snprintf(right, sizeof(right), "%s%s%s%d%%  %02d:%02d",
			 mark, gap, charging ? "+" : "", pct, tm.tm_hour,
			 tm.tm_min);
	else
		snprintf(right, sizeof(right), "%s%s%02d:%02d", mark, gap,
			 tm.tm_hour, tm.tm_min);

	int rw = ktui_utf8_width(right);
	int right_x = w - rw - 1;
	if (right_x > x)
		ktui_draw_text_right(0, 0, w - 1, right, KT_TEXT, KT_SURFACE,
				     KT_A_NONE);

	/*
	 * ── the recording indicator, left of everything on the right ──
	 *
	 * The one thing on this panel that is not information but a WARNING, so
	 * it takes the secondary colour and, for the camera, reverse video as
	 * well: a microphone is a thing you can be recorded by and a camera is a
	 * thing you can be seen by, and those do not deserve the same weight.
	 * It names ONE app and counts the rest — the panel is one row, and
	 * "MIC firefox +2" is more useful than three truncated names.
	 */
	for (int kind = SH_PRIV_MIC; kind <= SH_PRIV_CAM; kind++) {
		int n = sh_priv_count(sh, kind);
		const char *who = n > 0 ? sh_priv_name(sh, kind) : NULL;
		char label[48];

		if (n <= 0)
			continue;
		if (n > 1)
			snprintf(label, sizeof(label), "%s%s %.14s +%d",
				 ktui_glyph[KT_G_BULLET],
				 kind == SH_PRIV_MIC ? "MIC" : "CAM",
				 who ? who : "?", n - 1);
		else
			snprintf(label, sizeof(label), "%s%s %.20s",
				 ktui_glyph[KT_G_BULLET],
				 kind == SH_PRIV_MIC ? "MIC" : "CAM",
				 who ? who : "?");

		int lw = ktui_utf8_width(label);
		if (right_x - x < lw + 2)
			break;
		right_x -= lw + 1;
		ktui_draw_text(right_x, 0, lw, label, KT_WARN, KT_SURFACE,
			       kind == SH_PRIV_CAM ? KT_A_REVERSE : KT_A_NONE);
	}

	/*
	 * ── the tray, immediately left of the clock ──
	 *
	 * One cell per item and no icon: this is a character grid, and the first
	 * letter of an item's Id is what survives the translation. Status is the
	 * colour — passive dim, active in the text colour, NeedsAttention in the
	 * accent AND reversed, because "this one wants you" has to be visible
	 * without a second glance on an eight-colour palette.
	 */
	int ntray = sh_tray_count(sh);
	sh->tray_hit_x = sh->tray_hit_end = 0;
	if (ntray > 0 && right_x - x > ntray * 2 + 4) {
		int tx = right_x - ntray * 2 - 1;
		sh->tray_hit_x = tx;
		for (int i = 0; i < ntray; i++) {
			const struct sh_tray_item *it = sh_tray_get(sh, i);
			char cell[8];
			const char *src = it->id[0] ? it->id : it->service;
			/* One BYTE, not one codepoint: a leading multi-byte
			 * character would be cut in half and drawn as garbage.
			 * Ids are program names — ascii in every case that
			 * exists — so the fallback is a dot rather than a
			 * decoder. */
			snprintf(cell, sizeof(cell), "%c",
				 (unsigned char)*src < 0x80 ? *src : '.');
			if (cell[0] >= 'a' && cell[0] <= 'z')
				cell[0] = (char)(cell[0] - 'a' + 'A');
			int fg = it->status == SH_TRAY_ATTENTION ? KT_ACCENT
				 : it->status == SH_TRAY_ACTIVE     ? KT_TEXT
								    : KT_DIM;
			ktui_draw_text(tx, 0, 1, cell, fg, KT_SURFACE,
				       it->status == SH_TRAY_ATTENTION
					       ? KT_A_REVERSE
					       : KT_A_NONE);
			tx += 2;
		}
		sh->tray_hit_end = tx;
		right_x = sh->tray_hit_x - 1;
	}

	/* ── middle: the windows, in whatever room is left ── */
	int avail = right_x - x - 1;
	sh->task_hit_x = x;
	sh->task_cell_w = 0;
	if (avail > 4 && sh->ntasks > 0) {
		/*
		 * Every entry gets the same width, so clicking position N always
		 * means task N. A proportional layout would need the click map
		 * rebuilt on every title change, and a title changes whenever a
		 * browser tab does.
		 */
		int per = avail / sh->ntasks;
		if (per > 20)
			per = 20;
		if (per >= 4) {
			sh->task_cell_w = per;
			for (int i = 0; i < sh->ntasks && x + per <= right_x; i++) {
				const struct sh_task *t = &sh->tasks[i];
				const char *label = t->app_id[0] ? t->app_id
								 : t->title;
				int fg = t->activated ? KT_ACCENT
						      : t->minimized ? KT_DIM
								     : KT_TEXT;
				ktui_draw_text(x, 0, per - 1, label, fg,
					       KT_SURFACE,
					       t->activated ? KT_A_REVERSE
							    : KT_A_NONE);
				x += per;
			}
		}
	}

	ktui_draw_flush();
}


/* ── the bottom panel ──────────────────────────────────────────────────────
 *
 *   [□ foot] [■ GIMP] [□ mc]                              ▪▫▫▫    ░
 *
 * GNOME 2's second panel: the window list on the left, the workspace pager and
 * show-desktop on the right. A SEPARATE FUNCTION rather than a mode inside
 * draw_panel(), because the top panel's layout is load-bearing — it lays out
 * right to left so the clock cannot be pushed off a narrow screen — and
 * threading a second set of applets through it would put both at risk to
 * change either.
 *
 * The two panels are two PROCESSES of the same binary. Each layer-shell surface
 * has one exclusive zone, so a single process cannot reserve space at the top
 * and the bottom; kdos-desktop-start launches `kdos-shell` and
 * `kdos-shell --bottom`.
 */
static void draw_bottom(struct sh_state *sh)
{
	int w = ktui_w, h = ktui_h;
	if (w < 20 || h < 1)
		return;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);

	/* Right first, for the same reason the top panel does it: the pager has
	 * a fixed width and the window list is what has to give. */
	int show_x = w - 2;
	ktui_draw_text(show_x, 0, 1, ktui_glyph[KT_G_SHADE], KT_DIM, KT_SURFACE,
		       KT_A_NONE);
	sh->show_hit_x = show_x;

	int pager_w = sh->nws * 2;
	int pager_x = show_x - pager_w - 3;
	sh->pager_hit_x = pager_x;
	for (int i = 0; i < sh->nws; i++) {
		/*
		 * A filled square for the active workspace, a hollow one for the
		 * rest. Not the digits the top panel uses: this is a pager, the
		 * shape carries the meaning, and two glyphs read faster than
		 * four numbers when you are aiming rather than reading.
		 */
		bool active = i == sh->active_ws;
		ktui_draw_text(pager_x + i * 2, 0, 1,
			       active ? ktui_glyph[KT_G_SQUARE] : ktui_glyph[KT_G_DOT],
			       active ? KT_ACCENT : KT_DIM, KT_SURFACE, KT_A_NONE);
	}
	sh->pager_hit_end = pager_x + pager_w;

	/* ── the window list ── */
	int x = 1;
	int avail = pager_x - x - 1;
	sh->task_hit_x = x;
	sh->task_cell_w = 0;
	if (avail <= 6 || sh->ntasks <= 0) {
		ktui_draw_flush();
		return;
	}

	/*
	 * Equal widths, so clicking position N always means task N — the same
	 * decision the top panel's list makes, and for the same reason: a
	 * proportional layout would need the click map rebuilt whenever a
	 * browser tab changed a title.
	 */
	int per = avail / sh->ntasks;
	if (per > 24)
		per = 24;
	if (per < 6)
		per = 6;
	sh->task_cell_w = per;

	for (int i = 0; i < sh->ntasks && x + per <= pager_x; i++) {
		const struct sh_task *t = &sh->tasks[i];
		int fg = t->activated ? KT_SURFACE : KT_TEXT;
		int bg = t->activated ? KT_ACCENT : KT_SURFACE;

		ktui_draw_fill(krect(x, 0, per - 1, 1), bg);
		/*
		 * A filled square is a window you can see, a hollow one is
		 * minimised. Every other desktop draws a minimised entry in
		 * italics or greyed, and neither survives eight colours and one
		 * font weight.
		 */
		ktui_draw_text(x, 0, 1,
			       t->minimized ? ktui_glyph[KT_G_DOT]
					    : ktui_glyph[KT_G_SQUARE],
			       t->minimized ? KT_DIM : fg, bg, KT_A_NONE);

		const char *label = t->title[0] ? t->title : t->app_id;
		ktui_draw_text(x + 2, 0, per - 3, label, fg, bg, KT_A_NONE);
		x += per;
	}
	ktui_draw_flush();
}

/* Clicks on the bottom panel. Kept beside its drawing for the same reason the
 * two are split at all: the hit map is what the last frame actually drew. */
static void handle_click_bottom(struct sh_state *sh, int cx, int btn)
{
	if (btn != SH_TRAY_BTN_LEFT)
		return;

	if (cx >= sh->show_hit_x) {
		/*
		 * Show desktop = minimise everything. There is no "restore them
		 * all" here, and that is honest rather than lazy: kdos-comp has
		 * no iconified state of its own, so the panel would have to
		 * remember what it hid, and a memory that goes stale the moment
		 * a window closes is worse than a button that does one thing.
		 */
		for (int i = 0; i < sh->ntasks; i++)
			sh_minimize_task(sh, i);
		return;
	}
	if (cx >= sh->pager_hit_x && cx < sh->pager_hit_end) {
		int i = (cx - sh->pager_hit_x) / 2;
		if (i >= 0 && i < sh->nws)
			sh_activate_workspace(sh, i);
		return;
	}
	if (sh->task_cell_w > 0 && cx >= sh->task_hit_x) {
		int i = (cx - sh->task_hit_x) / sh->task_cell_w;
		if (i >= 0 && i < sh->ntasks)
			sh_activate_task(sh, i);
	}
}

/* ── clicks ────────────────────────────────────────────────────────────── */

static void handle_click(struct sh_state *sh, int cx, int btn)
{
	if (sh->tray_hit_end > sh->tray_hit_x && cx >= sh->tray_hit_x &&
	    cx < sh->tray_hit_end) {
		int i = (cx - sh->tray_hit_x) / 2;
		/* The item is told where the pointer was in PIXELS: an app that
		 * pops a menu at the cursor gets the cursor, and one that
		 * ignores the argument loses nothing. */
		sh_tray_activate(sh, i, btn, cx * kwl_cell_w(), kwl_cell_h());
		return;
	}
	if (btn != SH_TRAY_BTN_LEFT)
		return;
	for (int i = 0; i < SH_NMENUS; i++)
		if (cx >= sh->menu_hit_x[i] && cx < sh->menu_hit_end[i]) {
			sh_spawn_menu(i);
			return;
		}
	if (cx >= sh->ws_hit_x && cx < sh->ws_hit_end) {
		/* Two cells per workspace: the digit and its separator. */
		int i = (cx - sh->ws_hit_x) / 2;
		if (i >= 0 && i < sh->nws)
			sh_activate_workspace(sh, i);
		return;
	}
	if (sh->task_cell_w > 0 && cx >= sh->task_hit_x) {
		int i = (cx - sh->task_hit_x) / sh->task_cell_w;
		if (i >= 0 && i < sh->ntasks)
			sh_activate_task(sh, i);
	}
}

int panel_main(int argc, char **argv)
{
	const char *font = NULL;
	int edge = KWL_EDGE_TOP;
	int dump = 0, dump_w = 100;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-width") && i + 1 < argc)
			dump_w = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--bottom"))
			edge = KWL_EDGE_BOTTOM;
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--version")) {
			printf("kdos-shell 0.1.0\n");
			return 0;
		} else {
			fprintf(stderr,
				"usage: kdos-shell [--bottom] [--font NAME]\n"
				"       kdos-shell --dump [--dump-width N]\n");
			return 2;
		}
	}

	/*
	 * Which panel this process IS.
	 *
	 * The two panels are two processes of the same binary: a layer-shell
	 * surface has ONE exclusive zone, so a single process cannot reserve
	 * space at the top and at the bottom.
	 */
	const int is_bottom = edge == KWL_EDGE_BOTTOM;

	struct sh_state sh = {0};
	/* -1, not 0: a zeroed struct would light "Applications" for the whole
	 * session, which reads as a menu that is stuck open. Nothing sets this
	 * yet — the menu is a separate process and does not report back — so it
	 * stays -1 and the bar is drawn unlit. */
	sh.menu_open = -1;
	KwlConfig cfg = {
		.role = KWL_ROLE_PANEL,
		.edge = edge,
		.cells = 1,
		/* Must equal the .desktop id or the shell shows a second, unnamed
		 * icon for itself — the bug `kdos appid` exists to catch, and the
		 * one program with no excuse for it. */
		.app_id = "kdos-shell",
		.font = font,
		.exclusive = 1,
	};

	sh_theme_from_cache();

	/*
	 * `--dump` renders one frame offscreen and prints the cell grid.
	 *
	 * It is the same draw_panel() the surface uses — not a second
	 * description of what the panel contains, which would be a second thing
	 * to keep true. That makes the panel testable without a screen, which
	 * is the whole of N15: this is testability, not a machine interface.
	 *
	 * It reads REAL protocol state, so it needs the connection; it just does
	 * not need a surface.
	 */
	if (dump) {
		cfg.role = KWL_ROLE_NONE;
		if (kwl_init(&cfg) != 0) {
			fprintf(stderr, "kdos-shell: no compositor to dump\n");
			return 1;
		}
		if (sh_connect(&sh) != 0) {
			fprintf(stderr, "kdos-shell: no window list to dump\n");
			kwl_shutdown();
			return 1;
		}
		/* The tray too: a dump that omits it is a dump of a panel
		 * nobody has. Failing to reach a bus is not an error here. */
		sh_tray_init(&sh);
		sh_tray_dispatch(&sh);
		sh_priv_init(&sh);
		sh_priv_settle(&sh, 800);
		sh_priv_dispatch(&sh);
		if (dump_w < 20 || dump_w > 500)
			dump_w = 100;
		ktui_offscreen_init(dump_w, 1);
		if (is_bottom)
			draw_bottom(&sh);
		else
			draw_panel(&sh);
		ktui_draw_dump();
		sh_priv_free(&sh);
		sh_tray_free(&sh);
		sh_disconnect(&sh);
		kwl_shutdown();
		return 0;
	}

	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-shell: no compositor, no font, or no "
				"layer-shell — not starting\n");
		return 1;
	}
	if (sh_connect(&sh) != 0) {
		fprintf(stderr, "kdos-shell: the compositor exposes no window "
				"list; the panel would be blank\n");
		kwl_shutdown();
		return 1;
	}
	/* No session bus is a session with no tray, not a shell that refuses to
	 * start — a tty-launched panel for a screenshot has neither. */
	sh_tray_init(&sh);
	/* Same contract: no PipeWire is a session where nothing can be recording
	 * through it, not a shell that refuses to start. */
	sh_priv_init(&sh);
	ktui_draw_init();

	while (!kwl_should_close()) {
		if (is_bottom)
			draw_bottom(&sh);
		else
			draw_panel(&sh);

		KtuiEvent ev;
		/*
		 * A second, not a frame rate. The panel's fastest-changing thing
		 * is a clock that ticks once a minute; redrawing at display rate
		 * would burn a core to show the same pixels. Any event — a
		 * window appearing, a click — wakes it sooner.
		 */
		if (ktui_backend()->poll_event(&ev, 1000)) {
			if (ev.type == KT_EVT_MOUSE &&
			    ev.press == KT_MP_PRESS &&
			    (ev.btn == KT_MB_LEFT || ev.btn == KT_MB_MIDDLE ||
			     ev.btn == KT_MB_RIGHT))
				(is_bottom ? handle_click_bottom
					   : handle_click)(&sh, ev.mx,
					     ev.btn == KT_MB_MIDDLE
						     ? SH_TRAY_BTN_MIDDLE
					     : ev.btn == KT_MB_RIGHT
						     ? SH_TRAY_BTN_RIGHT
						     : SH_TRAY_BTN_LEFT);
		}
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}
		sh_dispatch(&sh);
		sh_tray_dispatch(&sh);
		sh_priv_dispatch(&sh);
	}

	sh_priv_free(&sh);
	sh_tray_free(&sh);
	sh_disconnect(&sh);
	kwl_shutdown();
	return 0;
}
