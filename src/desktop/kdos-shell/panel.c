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
 *   ║ ▶ KDOS │ 1 2 ▓3▓ 4 │ foot  firefox-esr  gimp │ ▂▄▆ 41% 21:07     ║
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
 * the kernel already exports as text.
 */

#include <dirent.h>
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

	/* ── left: the mark and the workspaces ── */
	int x = 1;
	x += ktui_draw_text(x, 0, w - x, "KDOS", KT_ACCENT, KT_SURFACE, KT_A_NONE);
	x += 1;
	x += ktui_draw_text(x, 0, w - x, ktui_glyph[KT_G_VL], KT_DIM,
			    KT_SURFACE, KT_A_NONE);
	x += 1;

	sh->ws_hit_x = x;
	for (int i = 0; i < sh->nws && x < w - 4; i++) {
		char label[8];
		snprintf(label, sizeof(label), "%d", i + 1);
		/*
		 * The active workspace is drawn REVERSED rather than merely in
		 * the accent: on the eight-slot palette a colour change alone
		 * is easy to miss, and reverse is the one emphasis that reads
		 * identically on a tty and in a truecolor surface.
		 */
		int active = i == sh->active_ws;
		int fg = sh->ws_occupied[i] ? KT_ACCENT : KT_DIM;
		x += ktui_draw_text(x, 0, w - x, label, active ? KT_SURFACE : fg,
				    active ? KT_ACCENT : KT_SURFACE,
				    active ? KT_A_REVERSE : KT_A_NONE);
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

/* ── clicks ────────────────────────────────────────────────────────────── */

static void handle_click(struct sh_state *sh, int cx)
{
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

	struct sh_state sh = {0};
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
		if (dump_w < 20 || dump_w > 500)
			dump_w = 100;
		ktui_offscreen_init(dump_w, 1);
		draw_panel(&sh);
		ktui_draw_dump();
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
	ktui_draw_init();

	while (!kwl_should_close()) {
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
			    ev.btn == KT_MB_LEFT && ev.press == KT_MP_PRESS)
				handle_click(&sh, ev.mx);
		}
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}
		sh_dispatch(&sh);
	}

	sh_disconnect(&sh);
	kwl_shutdown();
	return 0;
}
