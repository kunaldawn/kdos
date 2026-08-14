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
#include <sys/wait.h>
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

/*
 * Is this machine on a network, and by which interface.
 *
 * From /sys/class/net, like the battery and for the same reason: NetworkManager
 * is running and could be asked over D-Bus, but the kernel already exports the
 * answer as text and a panel does not need a bus round trip per second to read
 * a file. It reports the INTERFACE rather than an SSID — an SSID needs nl80211,
 * which is a netlink socket and a genl family lookup for a string that does not
 * fit in this row anyway. Clicking it opens the thing that CAN change it.
 *
 * Returns 1 when something is up, and fills `out` with its name either way.
 */
static int net_state(char *out, size_t n)
{
	DIR *d = opendir("/sys/class/net");
	struct dirent *e;
	int up = 0;

	snprintf(out, n, "down");
	if (!d)
		return 0;
	while ((e = readdir(d))) {
		char path[512], buf[64];
		if (e->d_name[0] == '.' || !strcmp(e->d_name, "lo"))
			continue;
		snprintf(path, sizeof(path), "/sys/class/net/%s/operstate",
			 e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) != 0)
			continue;
		if (strcmp(buf, "up"))
			continue;
		/* A wireless interface wins a tie: on a laptop with both, the
		 * one people ask about is the one that drops. */
		snprintf(path, sizeof(path), "/sys/class/net/%s/wireless",
			 e->d_name);
		int wifi = access(path, F_OK) == 0;
		if (!up || wifi) {
			snprintf(out, n, "%s", e->d_name);
			up = 1;
		}
		if (wifi)
			break;
	}
	closedir(d);
	return up;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * One applet on the right wing, laid out right to left.
 *
 * Returns 0 without drawing when there is not room, and records an EMPTY span
 * so a click lands on nothing rather than on whatever used to be there — a hit
 * map that outlives what it describes is how a narrow screen ends up muting
 * itself when somebody aims at the clock.
 */
static int applet(struct sh_state *sh, int id, int *right_x, int x_min,
		  const char *label, int fg, int attr)
{
	int lw = ktui_utf8_width(label);

	sh->ap_x[id] = sh->ap_end[id] = 0;
	if (!lw || *right_x - x_min < lw + 2)
		return 0;
	*right_x -= lw + 1;
	ktui_draw_text(*right_x, 0, lw, label, fg, KT_SURFACE, attr);
	sh->ap_x[id] = *right_x;
	sh->ap_end[id] = *right_x + lw;
	return 1;
}

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
		/* i == 0 keeps the span recorded BEFORE the ≡ mark, so the mark
		 * itself opens Applications — overwriting it here was the dead
		 * store that made the mark decorative. */
		if (i)
			sh->menu_hit_x[i] = x;
		/*
		 * Lit while the pointer is on it. `menu_open` cannot drive this
		 * — the menu is a separate process and does not report back —
		 * so what the bar shows is where the pointer is, which is the
		 * honest thing it knows and is what makes the three words read
		 * as buttons rather than as a caption.
		 */
		int lit = i == sh->menu_open || i == sh->hover_menu;
		x += ktui_draw_text(x, 0, w - x, sh_menu_labels[i],
				    lit ? KT_SURFACE : KT_TEXT,
				    lit ? KT_ACCENT : KT_SURFACE,
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
	for (int i = 0; i < SH_MAX_WS; i++)
		sh->ws_hit[i] = -1;	/* -1 is "not on screen this frame" */
	for (int i = 0; i < sh->nws && x < w - 4; i++) {
		char label[8];
		snprintf(label, sizeof(label), "%d", i + 1);
		sh->ws_hit[i] = x;
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

	snprintf(right, sizeof(right), "%02d:%02d", tm.tm_hour, tm.tm_min);
	int rw = ktui_utf8_width(right);
	int right_x = w - rw - 1;
	if (right_x > x) {
		ktui_draw_text_right(0, 0, w - 1, right, KT_TEXT, KT_SURFACE,
				     KT_A_NONE);
		sh->ap_x[SH_AP_CLOCK] = right_x;
		sh->ap_end[SH_AP_CLOCK] = w - 1;
	} else {
		sh->ap_x[SH_AP_CLOCK] = sh->ap_end[SH_AP_CLOCK] = 0;
	}

	/*
	 * ── the right wing's applets ──
	 *
	 * Everything here was a picture until now: a battery percentage, a clock
	 * and a mark, none of which answered a click, and no volume or network
	 * indicator at all — so a machine with no media keys had no way to change
	 * the volume from the desktop and nothing that said whether it was on a
	 * network. Each is a span the click handler reads back.
	 */
	int charging = 0, pct = battery_percent(&charging);
	char label[48];

	if (pct >= 0) {
		snprintf(label, sizeof(label), "%s%d%%", charging ? "+" : "",
			 pct);
		/* Colour is the warning, because there is no room for a word:
		 * urgent under 15% and only when nothing is charging it. */
		int fg = KT_TEXT;
		if (!charging && pct < 15)
			fg = KT_ERR;
		else if (!charging && pct < 30)
			fg = KT_WARN;
		applet(sh, SH_AP_BATT, &right_x, x, label, fg, KT_A_NONE);
	} else {
		sh->ap_x[SH_AP_BATT] = sh->ap_end[SH_AP_BATT] = 0;
	}

	int muted = 0, vol = sh_volume_get(&muted);
	if (vol >= 0) {
		if (muted)
			snprintf(label, sizeof(label), "VOL off");
		else
			snprintf(label, sizeof(label), "VOL %d%%", vol);
		applet(sh, SH_AP_VOL, &right_x, x, label,
		       muted ? KT_DIM : KT_TEXT, KT_A_NONE);
	} else {
		sh->ap_x[SH_AP_VOL] = sh->ap_end[SH_AP_VOL] = 0;
	}

	char iface[32];
	int up = net_state(iface, sizeof(iface));
	snprintf(label, sizeof(label), "NET %.8s", iface);
	applet(sh, SH_AP_NET, &right_x, x, label, up ? KT_TEXT : KT_DIM,
	       KT_A_NONE);

	if (restarts > 0) {
		/* A mark rather than a word: the panel is one row, and the point
		 * is to be noticed and clicked, not to explain itself in situ. */
		snprintf(label, sizeof(label), "%s", ktui_glyph[KT_G_BULLET]);
		applet(sh, SH_AP_RESTART, &right_x, x, label, KT_WARN,
		       KT_A_NONE);
	} else {
		sh->ap_x[SH_AP_RESTART] = sh->ap_end[SH_AP_RESTART] = 0;
	}

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
		/* The camera's emphasis is a FILL plus swapped slots, not
		 * KT_A_REVERSE: the attribute inverts only the cells the text
		 * covers, so `●CAM firefox` would come out as two lit blocks
		 * with an unlit gap — the same defect the focused task entry
		 * had. */
		if (kind == SH_PRIV_CAM) {
			ktui_draw_fill(krect(right_x, 0, lw, 1), KT_WARN);
			ktui_draw_text(right_x, 0, lw, label, KT_SURFACE,
				       KT_WARN, KT_A_NONE);
		} else {
			ktui_draw_text(right_x, 0, lw, label, KT_WARN,
				       KT_SURFACE, KT_A_NONE);
		}
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
				/*
				 * FILL, then draw with the slots swapped — not
				 * KT_A_REVERSE over the label. The attribute
				 * inverts the cells the TEXT occupies, so the
				 * spaces inside a name were left at the panel's
				 * own background and the focused entry came out
				 * as one highlighted block per WORD:
				 * `▓GNU▓ ▓Image▓ ▓Mani▓`. Invisible until a
				 * window with a space in its name was focused on
				 * a screen wide enough to show it, and the
				 * bottom panel has always done it this way.
				 */
				int fg = t->activated ? KT_SURFACE
						      : t->minimized ? KT_DIM
								     : KT_TEXT;
				int bg = t->activated ? KT_ACCENT : KT_SURFACE;
				/* Hover takes the accent WITHOUT the swap, so
				 * it cannot be read as the focused entry: it is
				 * an affordance, not a state. */
				if (!t->activated && sh->hover_task == i)
					fg = KT_ACCENT;
				if (t->activated)
					ktui_draw_fill(krect(x, 0, per - 1, 1), bg);
				ktui_draw_text(x, 0, per - 1, sh_task_label(t),
					       fg, bg, KT_A_NONE);
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
 * and the bottom; the compositor supervises both (`kdos-child.c`), and
 * `panel_bottom = no` in comp.conf is what turns this one off. It said
 * kdos-desktop-start launched it for a release, and nothing did.
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

		/* Hover in the accent, unreversed: an entry the pointer is on
		 * is not an entry that has focus, and on eight colours the
		 * difference has to be the swap rather than a fifth shade. */
		if (!t->activated && sh->hover_task == i)
			fg = KT_ACCENT;

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

		ktui_draw_text(x + 2, 0, per - 3, sh_task_label(t), fg, bg,
			       KT_A_NONE);
		x += per;
	}
	ktui_draw_flush();
}

/* Clicks on the bottom panel. Kept beside its drawing for the same reason the
 * two are split at all: the hit map is what the last frame actually drew. */
static void handle_click_bottom(struct sh_state *sh, int cx, int btn)
{
	/* Same three buttons as the top panel's list, and the same reason. */
	if (sh->task_cell_w > 0 && cx >= sh->task_hit_x &&
	    cx < sh->pager_hit_x) {
		int i = (cx - sh->task_hit_x) / sh->task_cell_w;
		if (i < 0 || i >= sh->ntasks)
			return;
		if (btn == SH_TRAY_BTN_MIDDLE)
			sh_close_task(sh, i);
		else if (btn == SH_TRAY_BTN_RIGHT)
			sh_minimize_task(sh, i);
		else
			sh_toggle_task(sh, i);
		return;
	}
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

/*
 * Which task entry, or -1. One place, because the draw, the click and the
 * hover all have to agree about it and two of them disagreeing is a panel that
 * highlights one window and raises another.
 */
static int task_at(const struct sh_state *sh, int cx)
{
	if (sh->task_cell_w <= 0 || cx < sh->task_hit_x)
		return -1;
	int i = (cx - sh->task_hit_x) / sh->task_cell_w;
	return i >= 0 && i < sh->ntasks ? i : -1;
}

/*
 * Which workspace digit is at this cell, or -1.
 *
 * Reads the positions the last frame RECORDED rather than dividing by two. The
 * stride is two cells only while every label is one digit wide: with
 * `<desktops number="12"/>` in rc.xml — a supported thing to write — everything
 * from the tenth on was offset by one more cell and the strip activated the
 * wrong workspace.
 */
static int ws_at(const struct sh_state *sh, int cx)
{
	for (int i = 0; i < sh->nws && i < SH_MAX_WS; i++) {
		if (sh->ws_hit[i] < 0)
			continue;
		int end = (i + 1 < sh->nws && sh->ws_hit[i + 1] >= 0)
				  ? sh->ws_hit[i + 1]
				  : sh->ws_hit_end;
		if (cx >= sh->ws_hit[i] && cx < end)
			return i;
	}
	return -1;
}

/* Which right-wing applet is at this cell, or -1. */
static int applet_at(const struct sh_state *sh, int cx)
{
	for (int i = 0; i < SH_AP_N; i++)
		if (sh->ap_end[i] > sh->ap_x[i] && cx >= sh->ap_x[i] &&
		    cx < sh->ap_end[i])
			return i;
	return -1;
}

static int menu_at(const struct sh_state *sh, int cx)
{
	for (int i = 0; i < SH_NMENUS; i++)
		if (cx >= sh->menu_hit_x[i] && cx < sh->menu_hit_end[i])
			return i;
	return -1;
}

/* The pointer moved. Nothing here acts; it only records what is under it, so
 * the next frame can light it. The bottom panel has a window list too and no
 * menu bar, which is the only difference. */
static void handle_motion(struct sh_state *sh, int cx, int is_bottom)
{
	sh->hover_menu = is_bottom ? -1 : menu_at(sh, cx);
	sh->hover_task = task_at(sh, cx);
	if (is_bottom && cx >= sh->pager_hit_x)
		sh->hover_task = -1;
}

/*
 * The wheel over the workspace strip steps through workspaces, which is what
 * every pager on every desktop does and costs one branch. It is also the only
 * way to reach workspace 5 and up: the strip has room for four digits before
 * the window list starts and the numbers do not scroll.
 */
static void handle_wheel(struct sh_state *sh, int cx, int up)
{
	/* Over the volume applet the wheel is the volume, which is what a
	 * pointer over a volume readout is for on every desktop that has one. */
	if (applet_at(sh, cx) == SH_AP_VOL) {
		int muted = 0, vol = sh_volume_get(&muted);
		if (vol >= 0)
			sh_volume_set(vol + (up ? 5 : -5));
		return;
	}
	if (cx < sh->ws_hit_x || cx >= sh->ws_hit_end || sh->nws < 2)
		return;
	int i = sh->active_ws + (up ? -1 : 1);
	if (i < 0)
		i = sh->nws - 1;
	if (i >= sh->nws)
		i = 0;
	sh_activate_workspace(sh, i);
}

/*
 * Spawn something the panel must not wait for. Double-forked, so the panel
 * neither reaps nor blocks — the same shape sh_spawn_menu uses, and the reason
 * is the same: a clock that stopped while a terminal started would be worse
 * than no button at all.
 */
static void panel_spawn(const char *const argv[])
{
	pid_t pid = fork();
	if (pid == 0) {
		if (fork() == 0) {
			setsid();
			execvp(argv[0], (char *const *)argv);
			_exit(127);
		}
		_exit(0);
	} else if (pid > 0) {
		waitpid(pid, NULL, 0);
	}
}

/*
 * A click on the right wing.
 *
 * Each applet does the one thing a person aiming at it wants: the volume mutes,
 * the network opens the tool that can change it, the clock shows a calendar,
 * the restart mark explains itself. Nothing here opens a settings application,
 * because there isn't one — these are the whole of the desktop's controls.
 */
static void handle_applet(struct sh_state *sh, int id, int btn)
{
	switch (id) {
	case SH_AP_VOL:
		if (btn == SH_TRAY_BTN_LEFT)
			sh_volume_toggle();
		break;
	case SH_AP_NET: {
		const char *argv[] = { "foot", "-e", "nmtui", NULL };
		panel_spawn(argv);
		break;
	}
	case SH_AP_CLOCK:
	case SH_AP_BATT: {
		/* Anchored under itself, in pixels, one panel down — the same
		 * trick the menu bar uses, and layer-shell's only coordinate. */
		char xs[16], ys[16];
		snprintf(xs, sizeof(xs), "%d",
			 sh->ap_x[id] * kwl_cell_w());
		snprintf(ys, sizeof(ys), "%d", ktui_h * kwl_cell_h());
		const char *argv[] = { "kdos-cal", "--at", xs, ys, NULL };
		panel_spawn(argv);
		break;
	}
	case SH_AP_RESTART: {
		const char *argv[] = { "foot", "-e", "kdos", "restarts", NULL };
		panel_spawn(argv);
		break;
	}
	default:
		break;
	}
}

static void handle_click(struct sh_state *sh, int cx, int btn)
{
	int ap = applet_at(sh, cx);
	if (ap >= 0) {
		handle_applet(sh, ap, btn);
		return;
	}

	if (sh->tray_hit_end > sh->tray_hit_x && cx >= sh->tray_hit_x &&
	    cx < sh->tray_hit_end) {
		int i = (cx - sh->tray_hit_x) / 2;
		/* The item is told where the pointer was in PIXELS: an app that
		 * pops a menu at the cursor gets the cursor, and one that
		 * ignores the argument loses nothing. */
		sh_tray_activate(sh, i, btn, cx * kwl_cell_w(), kwl_cell_h());
		return;
	}

	/*
	 * The window list answers all three buttons, the way a taskbar has
	 * since Windows 95: left toggles, middle closes, right minimises. There
	 * is no per-window menu to open — that is the compositor's client-menu,
	 * reachable on the window itself — so right does the one thing a menu
	 * would have been opened for.
	 */
	int task = task_at(sh, cx);
	if (task >= 0) {
		if (btn == SH_TRAY_BTN_MIDDLE)
			sh_close_task(sh, task);
		else if (btn == SH_TRAY_BTN_RIGHT)
			sh_minimize_task(sh, task);
		else
			sh_toggle_task(sh, task);
		return;
	}

	if (btn != SH_TRAY_BTN_LEFT)
		return;

	int menu = menu_at(sh, cx);
	if (menu >= 0) {
		/* Anchored under the word, in pixels, one row down — the panel
		 * is `cells` tall and the menu hangs off the bottom of it. */
		sh_spawn_menu(menu, sh->menu_hit_x[menu] * kwl_cell_w(),
			      ktui_h * kwl_cell_h());
		return;
	}
	int ws = ws_at(sh, cx);
	if (ws >= 0)
		sh_activate_workspace(sh, ws);
}

int panel_main(int argc, char **argv)
{
	const char *font = NULL;
	const char *output = NULL;
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
		/* Which screen this panel is the panel of. The compositor
		 * supervises one per output and names it here; without it
		 * layer-shell picks one output and the others get nothing. */
		else if (!strcmp(argv[i], "--output") && i + 1 < argc)
			output = argv[++i];
		else if (!strcmp(argv[i], "--version")) {
			printf("kdos-shell 0.1.0\n");
			return 0;
		} else {
			fprintf(stderr,
				"usage: kdos-shell [--bottom] [--output NAME] "
				"[--font NAME]\n"
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
	sh.hover_menu = -1;
	sh.hover_task = -1;
	KwlConfig cfg = {
		.role = KWL_ROLE_PANEL,
		.edge = edge,
		.cells = 1,
		/* Must equal the .desktop id or the shell shows a second, unnamed
		 * icon for itself — the bug `kdos appid` exists to catch, and the
		 * one program with no excuse for it. */
		.app_id = "kdos-shell",
		.font = font,
		.output = output,
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
	/* `kdos theme <accent>` SIGHUPs us; see sh_theme_watch(). */
	sh_theme_watch();

	while (!kwl_should_close()) {
		if (sh_theme_dirty) {
			sh_theme_dirty = 0;
			sh_theme_from_cache();
			ktui_draw_invalidate();
		}
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
		if (ktui_backend()->poll_event(&ev, 1000) &&
		    ev.type == KT_EVT_MOUSE) {
			/* Plain movement arrives as KT_MP_DRAG — libkwl's
			 * spelling, and the same one kdos-menu reads. */
			if (ev.press == KT_MP_DRAG) {
				handle_motion(&sh, ev.mx, is_bottom);
			} else if (ev.press == KT_MP_PRESS) {
				if (ev.btn == KT_MB_WHEEL_UP ||
				    ev.btn == KT_MB_WHEEL_DOWN) {
					if (!is_bottom)
						handle_wheel(&sh, ev.mx,
							     ev.btn == KT_MB_WHEEL_UP);
				} else if (ev.btn == KT_MB_LEFT ||
					   ev.btn == KT_MB_MIDDLE ||
					   ev.btn == KT_MB_RIGHT) {
					(is_bottom ? handle_click_bottom
						   : handle_click)(&sh, ev.mx,
						     ev.btn == KT_MB_MIDDLE
							     ? SH_TRAY_BTN_MIDDLE
						     : ev.btn == KT_MB_RIGHT
							     ? SH_TRAY_BTN_RIGHT
							     : SH_TRAY_BTN_LEFT);
				}
			}
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
