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
 *   ║ ▶ KDOS │ 1 2 ▓3▓ 4 │ foot ×2  firefox-esr  gimp │ ●MIC firefox  K N  41% 21:07 ║
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
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kwl.h"
#include "shell.h"

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
		/* Restarted by hand: the SIGHUP `kdos theme` sends is caught
		 * without SA_RESTART (see sh_theme_watch), and a waitpid it
		 * interrupts leaves the intermediate child a zombie for the life
		 * of the panel — one per menu, applet or favorite click. */
		while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
			;
	}
}

/* ── the right wing: clock, battery, and what else is measurable ───────── */

/*
 * Battery, from /sys/class/power_supply — ALL of them.
 *
 * The first `type=Battery` entry is as likely to be a Bluetooth mouse's cell as
 * the laptop's pack (`scope=Device` is how the kernel marks a peripheral's),
 * and dual-battery ThinkPads export BAT0 and BAT1 that only mean anything
 * summed. Reported from charge_now/charge_full where the kernel gives them —
 * the only form that aggregates correctly — and from averaged `capacity`
 * otherwise.
 *
 * `discharging` is the flag the warning colours and the battery policy read,
 * and it is NOT merely "status says Discharging": a battery held at its charge
 * threshold reports "Not charging" on wall power, and painting that as dying
 * is the defect. Anything that says Charging/Full/Not charging, or any online
 * AC supply, means the machine is not running down.
 */
static int battery_percent(int *charging, int *discharging)
{
	DIR *d = opendir("/sys/class/power_supply");
	struct dirent *e;
	long now_sum = 0, full_sum = 0;
	long cap_sum = 0;
	int ncap = 0, any_batt = 0, drains = 0, powered = 0;

	*charging = 0;
	*discharging = 0;
	if (!d)
		return -1;
	while ((e = readdir(d))) {
		char path[512], buf[64];
		if (e->d_name[0] == '.')
			continue;
		snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type",
			 e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) != 0)
			continue;
		if (strcmp(buf, "Battery")) {
			/* Mains/USB/Wireless: online means wall power. */
			snprintf(path, sizeof(path),
				 "/sys/class/power_supply/%s/online", e->d_name);
			if (sh_read_line(path, buf, sizeof(buf)) == 0 &&
			    atoi(buf))
				powered = 1;
			continue;
		}

		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/scope", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0 &&
		    !strcmp(buf, "Device"))
			continue;	/* a mouse's cell, not the machine's */
		any_batt = 1;

		long bnow = -1, bfull = -1;
		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/charge_now", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0)
			bnow = atol(buf);
		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/charge_full", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0)
			bfull = atol(buf);
		if (bnow >= 0 && bfull > 0) {
			now_sum += bnow;
			full_sum += bfull;
		} else {
			snprintf(path, sizeof(path),
				 "/sys/class/power_supply/%s/capacity",
				 e->d_name);
			if (sh_read_line(path, buf, sizeof(buf)) == 0) {
				cap_sum += atoi(buf);
				ncap++;
			}
		}

		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/status", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0) {
			if (!strcmp(buf, "Charging")) {
				*charging = 1;
				powered = 1;
			} else if (!strcmp(buf, "Full") ||
				   !strcmp(buf, "Not charging")) {
				powered = 1;
			} else if (!strcmp(buf, "Discharging")) {
				drains = 1;
			}
		}
	}
	closedir(d);
	if (!any_batt)
		return -1;
	*discharging = drains && !powered;
	if (full_sum > 0)
		return (int)(now_sum * 100 / full_sum);
	if (ncap > 0)
		return (int)(cap_sum / ncap);
	return -1;
}

/*
 * What happens as the battery runs down, beyond a colour: one warning at 10%
 * and a suspend at 3%, each latched so a percentage that hovers on the line
 * does not fire once per frame. The latches reset the moment the machine is
 * not discharging — plugging in and unplugging again re-arms both, which is
 * the behaviour a person expects of a warning.
 */
static void battery_policy(struct sh_state *sh, int pct, int discharging)
{
	static int warned, suspended;

	if (!discharging) {
		warned = 0;
		suspended = 0;
		return;
	}
	if (pct < 0)
		return;
	if (pct <= 3 && !suspended) {
		suspended = 1;
		const char *argv[] = { "kdos-power", "suspend", NULL };
		panel_spawn(argv);
	} else if (pct <= 10 && !warned) {
		warned = 1;
		sh_tray_notify(sh, "Battery low",
			       "10% remaining. The machine suspends itself at 3%.");
	}
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
			snprintf(out, n, "%.*s", (int)n - 1, e->d_name);
			up = 1;
		}
		if (wifi)
			break;
	}
	closedir(d);
	return up;
}

/* ── favorites: the bottom panel's pinned launchers ────────────────────── */

/*
 * ~/.config/kdos/favorites — one desktop-entry id per line, resolved through
 * the same XDG search the taskbar labels use. A missing file is a panel with
 * no favorites row, not an error: pinning is opt-in.
 */
#define FAV_MAX 8

struct fav {
	char name[24];
	char exec[256];
};
static struct fav favs[FAV_MAX];
static int nfavs;
static int fav_x[FAV_MAX], fav_end[FAV_MAX];

static void load_favorites(void)
{
	char path[512], line[256];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	FILE *f;

	nfavs = 0;
	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%s/kdos/favorites", cfg);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.config/kdos/favorites", home);
	else
		return;
	f = fopen(path, "r");
	if (!f)
		return;
	while (nfavs < FAV_MAX && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\n")] = '\0';
		char *s = line;
		while (*s == ' ' || *s == '\t')
			s++;
		if (!*s || *s == '#')
			continue;
		struct fav *fv = &favs[nfavs];
		if (sh_desktop_entry(s, fv->name, sizeof(fv->name),
				     fv->exec, sizeof(fv->exec)) != 0 ||
		    !fv->exec[0])
			continue;	/* an id with no entry launches nothing */
		if (!fv->name[0])
			snprintf(fv->name, sizeof(fv->name), "%.*s",
				 (int)sizeof(fv->name) - 1, s);
		/* Stripped ONCE, here: a favorite has no document to
		 * substitute, and "%U" in argv opens as a search. */
		sh_strip_field_codes(fv->exec);
		if (fv->exec[0])
			nfavs++;
	}
	fclose(f);
}

static void fav_launch(int i)
{
	char buf[256];
	const char *argv[32];
	int n = 0;

	if (i < 0 || i >= nfavs)
		return;
	snprintf(buf, sizeof(buf), "%s", favs[i].exec);
	for (char *p = strtok(buf, " \t"); p && n < 31; p = strtok(NULL, " \t"))
		argv[n++] = p;
	argv[n] = NULL;
	if (n)
		panel_spawn(argv);
}

/* ── the window list: one chip per APP, not per window ─────────────────── */

/*
 * Windows are bucketed by app_id and the panel draws one chip per bucket —
 * Haiku's Deskbar answer, and the only one that scales: per-window entries at
 * 80 columns (the shipped 1280x800 with the 32px font) hit the width floor at
 * three windows and the taskbar VANISHED, which is the worst thing a taskbar
 * can do. A bucket of one keeps the old click semantics; a bucket of N is
 * labelled `Name ×N` and a left click opens kdos-menu's window list for it.
 *
 * The chip array is rebuilt every frame from the task list — cheap at 64
 * tasks, and the alternative is a cache invalidated by five different events.
 */
/*
 * The narrowest a chip may be. The bottom panel spends two of its cells on the
 * □/■ state marker, so it needs more. Named because the top panel's right wing
 * has to keep this much of the row clear for the list — see draw_panel.
 */
#define CHIP_MINW_TOP    4
#define CHIP_MINW_BOTTOM 6

struct chip {
	const char *label;
	char buf[48];
	int first;		/* task index of the first member */
	int count;
	int active;		/* any member focused */
	int allmin;		/* every member minimized */
};
static struct chip chips[SH_MAX_TASKS];
static int nchips;
static int chip_off;		/* first visible chip, wheel-shifted */
static int chip_vis;		/* chips drawn last frame */
static int plusn_x, plusn_end;	/* the +N overflow cell's span */
static int list_x0, list_x1;	/* the whole row's span, for the wheel */

static void build_chips(struct sh_state *sh)
{
	nchips = 0;
	for (int i = 0; i < sh->ntasks; i++) {
		const struct sh_task *t = &sh->tasks[i];
		struct chip *c = NULL;
		if (t->app_id[0]) {
			for (int j = 0; j < nchips; j++) {
				const struct sh_task *f =
					&sh->tasks[chips[j].first];
				if (f->app_id[0] &&
				    !strcmp(f->app_id, t->app_id)) {
					c = &chips[j];
					break;
				}
			}
		}
		if (!c) {
			c = &chips[nchips++];
			c->first = i;
			c->count = 0;
			c->active = 0;
			c->allmin = 1;
		}
		c->count++;
		if (t->activated)
			c->active = 1;
		if (!t->minimized)
			c->allmin = 0;
	}

	for (int i = 0; i < nchips; i++) {
		struct chip *c = &chips[i];
		const struct sh_task *t = &sh->tasks[c->first];
		if (c->count > 1) {
			snprintf(c->buf, sizeof(c->buf), "%s \xc3\x97%d",
				 sh_task_label(t), c->count);
			c->label = c->buf;
		} else {
			c->label = sh_task_label(t);
		}
	}

	/*
	 * Two single-window chips wearing the same Name are two apps a person
	 * cannot tell apart — different app_ids resolving to one entry Name.
	 * There the TITLE is the distinguishing half and it goes first.
	 */
	for (int i = 0; i < nchips; i++) {
		if (chips[i].count != 1)
			continue;
		const struct sh_task *ti = &sh->tasks[chips[i].first];
		if (!ti->title[0])
			continue;
		for (int j = 0; j < nchips; j++) {
			if (j == i || chips[j].count != 1)
				continue;
			if (!strcmp(sh_task_label(ti),
				    sh_task_label(&sh->tasks[chips[j].first]))) {
				chips[i].label = ti->title;
				break;
			}
		}
	}
}

/*
 * Draw the chip row into [x, limit). `marker` is the bottom panel's □/■ state
 * glyph; the top panel spends those two cells on label instead. Equal widths,
 * so clicking position N always means chip N — a proportional layout would
 * need the click map rebuilt whenever a browser tab changed a title.
 *
 * NEVER zero chips while there are windows: when they do not all fit, the
 * spare go behind a `+N` cell and the wheel (or a click on the cell) shifts
 * the window. That is the contract the old per-window list broke.
 */
static void draw_chips(struct sh_state *sh, int x, int limit, int marker)
{
	int avail = limit - x;
	int minw = marker ? CHIP_MINW_BOTTOM : CHIP_MINW_TOP;
	char pn[16];
	int pw;

	sh->task_hit_x = x;
	sh->task_cell_w = 0;
	chip_vis = 0;
	plusn_x = plusn_end = 0;
	list_x0 = list_x1 = 0;
	if (nchips <= 0 || avail < minw + 1)
		return;

	/* The overflow cell's REAL width, reserved before the chips divide the
	 * rest and measured at its widest (+N with everything but one hidden):
	 * a hardcoded three cells left `+12` drawn as a bare `+`, which says
	 * there are windows behind the cell and not how many. */
	snprintf(pn, sizeof(pn), "+%d", nchips - 1);
	pw = (int)strlen(pn);

	int nvis, hidden;
	if (nchips * minw <= avail) {
		nvis = nchips;
	} else {
		nvis = (avail - pw) / minw;
		if (nvis < 1)
			nvis = 1;
		if (nvis > nchips)
			nvis = nchips;
	}
	hidden = nchips - nvis;
	if (chip_off > nchips - nvis)
		chip_off = nchips - nvis;
	if (chip_off < 0)
		chip_off = 0;

	int per = (avail - (hidden ? pw : 0)) / nvis;
	if (per > (marker ? 24 : 20))
		per = marker ? 24 : 20;
	if (per < minw)
		per = minw;
	sh->task_cell_w = per;
	chip_vis = nvis;
	list_x0 = x;

	for (int k = 0; k < nvis && x + per <= limit; k++) {
		const struct chip *c = &chips[chip_off + k];
		/*
		 * FILL, then draw with the slots swapped — not KT_A_REVERSE
		 * over the label. The attribute inverts the cells the TEXT
		 * occupies, so the spaces inside a name were left at the
		 * panel's own background and the focused entry came out as one
		 * highlighted block per WORD: `▓GNU▓ ▓Image▓ ▓Mani▓`.
		 */
		int fg = c->active ? KT_SURFACE
				   : c->allmin ? KT_DIM : KT_TEXT;
		int bg = c->active ? KT_ACCENT : KT_SURFACE;
		/* Hover takes the accent WITHOUT the swap, so it cannot be
		 * read as the focused entry: an affordance, not a state. */
		if (!c->active && sh->hover_task == chip_off + k)
			fg = KT_ACCENT;
		ktui_draw_fill(krect(x, 0, per - 1, 1), bg);
		if (marker) {
			/* A filled square is a window you can see, a hollow
			 * one a group entirely minimised — italics and grey
			 * do not survive eight colours and one weight. */
			ktui_draw_text(x, 0, 1,
				       c->allmin ? ktui_glyph[KT_G_DOT]
						 : ktui_glyph[KT_G_SQUARE],
				       c->allmin ? KT_DIM : fg, bg, KT_A_NONE);
			ktui_draw_text(x + 2, 0, per - 3, c->label, fg, bg,
				       KT_A_NONE);
		} else {
			ktui_draw_text(x, 0, per - 1, c->label, fg, bg,
				       KT_A_NONE);
		}
		x += per;
	}
	if (hidden > 0) {
		snprintf(pn, sizeof(pn), "+%d", hidden);
		/* Whole or not at all: `per` is clamped up to minw on a row that
		 * cannot afford even one full chip, and what was left over then
		 * held the sign and none of the number. */
		if (limit - x >= (int)strlen(pn)) {
			plusn_x = x;
			x += ktui_draw_text(x, 0, limit - x, pn, KT_WARN,
					    KT_SURFACE, KT_A_NONE);
			plusn_end = x;
		}
	}
	list_x1 = x;
}

/* Which chip is at this cell, or -1 — the ABSOLUTE index, offset included. */
static int chip_at(const struct sh_state *sh, int cx)
{
	if (sh->task_cell_w <= 0 || chip_vis <= 0 || cx < sh->task_hit_x)
		return -1;
	int k = (cx - sh->task_hit_x) / sh->task_cell_w;
	if (k < 0 || k >= chip_vis)
		return -1;
	return chip_off + k;
}

/*
 * The wheel over the chip row. With overflow it shifts the window — the only
 * way to reach a hidden chip. Without it, it cycles activation through the
 * windows, fluxbox's oldest habit and one that costs nothing to keep.
 */
static void chips_wheel(struct sh_state *sh, int up)
{
	if (nchips > chip_vis && chip_vis > 0) {
		chip_off += up ? -1 : 1;
		if (chip_off < 0)
			chip_off = 0;
		if (chip_off > nchips - chip_vis)
			chip_off = nchips - chip_vis;
		return;
	}
	if (sh->ntasks < 2)
		return;
	int cur = -1;
	for (int i = 0; i < sh->ntasks; i++)
		if (sh->tasks[i].activated) {
			cur = i;
			break;
		}
	int nxt = cur < 0 ? 0
			  : (cur + (up ? -1 : 1) + sh->ntasks) % sh->ntasks;
	sh_activate_task(sh, nxt);
}

/*
 * The member list for a grouped chip: kdos-menu binds foreign-toplevel itself
 * and lists the titles (plus Close all / Minimize all). Anchored under the
 * chip on the top panel; the bottom panel omits the anchor — layer-shell
 * margins are measured from the top-left and this process does not know the
 * output's pixel height, so centred is the honest fallback.
 */
static void spawn_windows_menu(struct sh_state *sh, int ci, int is_bottom)
{
	char xs[16], ys[16];
	const char *app_id = sh->tasks[chips[ci].first].app_id;

	if (is_bottom) {
		const char *argv[] = { "kdos-menu", "--windows", app_id, NULL };
		panel_spawn(argv);
		return;
	}
	snprintf(xs, sizeof(xs), "%d",
		 (sh->task_hit_x + (ci - chip_off) * sh->task_cell_w) *
			 kwl_cell_w());
	snprintf(ys, sizeof(ys), "%d", ktui_h * kwl_cell_h());
	const char *argv[] = { "kdos-menu", "--windows", app_id, "--at", xs, ys,
			       NULL };
	panel_spawn(argv);
}

/*
 * A click on a chip. A bucket of one keeps the semantics every taskbar has had
 * since Windows 95: left toggles, middle closes politely, right minimises. A
 * bucket of N answers left with the member menu (which carries Close all, so
 * middle does nothing rather than something destructive) and right by
 * minimising the group.
 */
static void chip_click(struct sh_state *sh, int ci, int btn, int is_bottom)
{
	if (ci < 0 || ci >= nchips)
		return;
	const struct chip *c = &chips[ci];

	if (c->count == 1) {
		if (btn == SH_TRAY_BTN_MIDDLE)
			sh_close_task(sh, c->first);
		else if (btn == SH_TRAY_BTN_RIGHT)
			sh_minimize_task(sh, c->first);
		else
			sh_toggle_task(sh, c->first);
		return;
	}
	if (btn == SH_TRAY_BTN_RIGHT) {
		const char *id = sh->tasks[c->first].app_id;
		for (int i = 0; i < sh->ntasks; i++)
			if (!strcmp(sh->tasks[i].app_id, id))
				sh_minimize_task(sh, i);
		return;
	}
	if (btn == SH_TRAY_BTN_LEFT)
		spawn_windows_menu(sh, ci, is_bottom);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * Every span the click handlers read, emptied. Called at the TOP of both draw
 * paths, so an early return (a surface squeezed under 20 columns during a
 * resize) cannot leave the previous frame's hit map live — which is exactly
 * the "hit map outlives what it describes" defect the applet comment warns
 * about, shipped by the early return itself.
 */
static void clear_hits(struct sh_state *sh)
{
	for (int i = 0; i < SH_NMENUS; i++)
		sh->menu_hit_x[i] = sh->menu_hit_end[i] = 0;
	sh->ws_hit_x = sh->ws_hit_end = 0;
	for (int i = 0; i < SH_MAX_WS; i++)
		sh->ws_hit[i] = -1;
	sh->pager_hit_x = sh->pager_hit_end = 0;
	sh->show_hit_x = sh->show_hit_end = -1;
	sh->task_hit_x = 0;
	sh->task_cell_w = 0;
	sh->tray_hit_x = sh->tray_hit_end = 0;
	for (int i = 0; i < SH_AP_N; i++)
		sh->ap_x[i] = sh->ap_end[i] = 0;
	for (int i = 0; i < FAV_MAX; i++)
		fav_x[i] = fav_end[i] = 0;
	chip_vis = 0;
	plusn_x = plusn_end = 0;
	list_x0 = list_x1 = 0;
}

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

/* Set by panel_main from `--clock`; strftime, guarded at the call site. */
static const char *clock_fmt = "%H:%M";

/*
 * The ≡ mark, or the word it stands in for — the fallback shell.h promises and
 * did not have. U+2261 is not one of the console font's 512 glyphs and is in
 * neither the vt nor the ascii tier, so anywhere but a full font the first cell
 * of the panel came out as `?`. Resolved ONCE, from the same caps
 * ktui_ramp_init picks its ramps with; the hit span is recorded from what was
 * drawn, so a four-cell mark needs nothing else.
 */
static const char *menu_mark(void)
{
	static const char *mark;

	if (!mark)
		mark = (ktui_caps & KT_CAP_UTF8) && !(ktui_caps & KT_CAP_LINUXVT)
			       ? SH_MENU_MARK
			       : "KDOS";
	return mark;
}

/*
 * What the right wing draws, measured OUTSIDE the draw.
 *
 * These lived in draw_panel, and a draw pass that measures is a draw pass that
 * ACTS: battery_policy suspends the machine at 3% and posts a notification at
 * 10%, and `kdos-shell --dump` — the documented way to look at this panel — ran
 * both, on a live session bus. The other half is the same mistake from the
 * other side: an autohidden panel draws the edge instead of the panel, so a
 * policy inside the draw would never run at all once the pointer left.
 *
 * panel_measure() READS. panel_tick() reads and then acts, and only the
 * interactive loop calls it.
 */
static int panel_pct = -1, panel_charging, panel_discharging;
static int panel_restarts = -1;

static void panel_measure(void)
{
	panel_pct = battery_percent(&panel_charging, &panel_discharging);
	panel_restarts = sh_restart_poll();
}

static void panel_tick(struct sh_state *sh)
{
	/*
	 * Started once a minute, HARVESTED nonblockingly: `kdos restarts` walks
	 * every process's maps, and waiting for it froze the panel for the whole
	 * walk.
	 */
	static time_t last_restart_check;
	time_t now = time(NULL);

	if (now - last_restart_check > 60) {
		last_restart_check = now;
		sh_restart_begin();
	}
	panel_measure();
	battery_policy(sh, panel_pct, panel_discharging);
}

/*
 * The panel is drawn RIGHT TO LEFT after the left wing, because the right wing
 * has a fixed width and the window list is what has to give when the screen is
 * narrow. Laying out left to right and hoping instead is how a clock ends up
 * pushed off the edge of a 1280-wide display by one long window title.
 *
 * When even that is not enough — the shipped 80-column resolution with a few
 * windows open — the whole left menu bar collapses to the ≡ mark BEFORE any
 * chip is dropped: three menu words a person can reach through the mark are
 * worth less than the window list, which is what a panel IS. Hence the
 * two-pass shape: lay out in full, and if the chips would not all fit, lay
 * out again compact.
 *
 * And the applets keep a FLOOR clear for the list, which is the other half of
 * the same rule. Each applet that became affordable took its width off
 * right_x, so the room left for the chips was not monotonic in the panel's
 * width: at 29 columns one chip was drawn, at 30 `VOL 42%` fit instead and the
 * list came out EMPTY — with it the wheel, so there was no way to reach any
 * window from the panel at all.
 */
static void draw_panel(struct sh_state *sh)
{
	int w = ktui_w, h = ktui_h;

	clear_hits(sh);
	if (w < 20 || h < 1)
		return;

	build_chips(sh);

	/* ── measurements, once per frame, before any layout pass ── */
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);

	int muted = 0, vol = sh_volume_get(&muted);
	char iface[32];
	int up = net_state(iface, sizeof(iface));

	char clock[64];
	if (strftime(clock, sizeof(clock), clock_fmt, &tm) == 0)
		snprintf(clock, sizeof(clock), "%02d:%02d", tm.tm_hour,
			 tm.tm_min);

	for (int compact = 0; compact < 2; compact++) {
		clear_hits(sh);
		ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);

		/* ── left: the menu bar, then the workspaces ──
		 *
		 * GNOME 2's three words, behind the ≡ mark that stands in for
		 * the distributor logo an icon theme would have supplied. The
		 * mark is a button too — it opens Applications, which is what
		 * a person aiming at a logo in the corner expects — and in
		 * compact mode it is the whole menu bar.
		 */
		int x = 1;
		sh->menu_hit_x[0] = x;
		x += ktui_draw_text(x, 0, w - x, menu_mark(), KT_ACCENT,
				    KT_SURFACE, KT_A_NONE);
		x += 1;
		if (!compact) {
			for (int i = 0; i < SH_NMENUS; i++) {
				/* i == 0 keeps the span recorded BEFORE the ≡
				 * mark, so the mark itself opens Applications
				 * — overwriting it here was the dead store
				 * that made the mark decorative. */
				if (i)
					sh->menu_hit_x[i] = x;
				/*
				 * Lit while the pointer is on it. `menu_open`
				 * cannot drive this — the menu is a separate
				 * process and does not report back — so what
				 * the bar shows is where the pointer is, which
				 * is the honest thing it knows.
				 */
				int lit = i == sh->menu_open ||
					  i == sh->hover_menu;
				x += ktui_draw_text(x, 0, w - x,
						    sh_menu_labels[i],
						    lit ? KT_SURFACE : KT_TEXT,
						    lit ? KT_ACCENT : KT_SURFACE,
						    KT_A_NONE);
				sh->menu_hit_end[i] = x;
				x += 2;
			}
		} else {
			sh->menu_hit_end[0] = x - 1;
		}
		x += ktui_draw_text(x, 0, w - x, ktui_glyph[KT_G_VL], KT_DIM,
				    KT_SURFACE, KT_A_NONE);
		x += 1;

		/*
		 * ext-workspace-v1 has no "there are windows here" state —
		 * ACTIVE, URGENT and HIDDEN are all it carries — so occupancy
		 * is DERIVED: the workspace being shown is occupied exactly
		 * when a window is not minimized, since kdos-comp reports a
		 * window on another workspace as MINIMIZED. That is right for
		 * every workspace the user has visited and silent about the
		 * rest, which is the honest shape.
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
			char label[16];
			/* The compositor's own name for the workspace when it
			 * published one; the synthesized digit otherwise. By
			 * codepoint — the name came verbatim out of the user's
			 * rc.xml <names> and need not be ascii. */
			if (sh->ws_name[i][0])
				sh_utf8_trunc(label, sizeof(label),
					      sh->ws_name[i], 12);
			else
				snprintf(label, sizeof(label), "%d", i + 1);
			sh->ws_hit[i] = x;
			/*
			 * The active workspace is drawn REVERSED — slots
			 * swapped, never KT_A_REVERSE on top, which would swap
			 * them back. URGENT is the warning colour: its own
			 * state with its own bit, cleared by the protocol the
			 * moment it stops being true.
			 */
			int active = i == sh->active_ws;
			int fg = sh->ws_urgent[i] ? KT_WARN
				 : sh->ws_occupied[i] ? KT_ACCENT
						      : KT_DIM;
			int maxw = w - x;
			if (maxw > 8)
				maxw = 8;	/* a name is not a banner */
			x += ktui_draw_text(x, 0, maxw, label,
					    active ? KT_SURFACE : fg,
					    active ? KT_ACCENT : KT_SURFACE,
					    KT_A_NONE);
			x += 1;
		}
		sh->ws_hit_end = x;

		x += ktui_draw_text(x, 0, w - x, ktui_glyph[KT_G_VL], KT_DIM,
				    KT_SURFACE, KT_A_NONE);
		x += 1;

		/* The floor the right wing may not cross: enough for one chip and
		 * its separator, whenever there is a window to put in it. The
		 * clock is the one applet drawn above it — it is pinned, and it
		 * is five cells. */
		int floor_x = x + (nchips > 0 ? CHIP_MINW_TOP + 1 : 0);

		/* ── right: clock first, the one thing that must not move ── */
		int rw = ktui_utf8_width(clock);
		int right_x = w - rw - 1;
		if (right_x > x) {
			ktui_draw_text_right(0, 0, w - 1, clock, KT_TEXT,
					     KT_SURFACE, KT_A_NONE);
			sh->ap_x[SH_AP_CLOCK] = right_x;
			sh->ap_end[SH_AP_CLOCK] = w - 1;
		}

		/*
		 * ── the right wing's applets ──
		 *
		 * Each is a span the click handler reads back; an applet with
		 * no room records an empty one — and `floor_x` rather than `x`
		 * is what "no room" means here, so the last of the row belongs
		 * to the window list.
		 */
		char label[64];
		if (panel_pct >= 0) {
			snprintf(label, sizeof(label), "%s%d%%",
				 panel_charging ? "+" : "", panel_pct);
			/* Colour is the warning, because there is no room for
			 * a word — and only while actually running down: a
			 * battery held at its threshold on AC is not dying. */
			int fg = KT_TEXT;
			if (panel_discharging && panel_pct < 15)
				fg = KT_ERR;
			else if (panel_discharging && panel_pct < 30)
				fg = KT_WARN;
			applet(sh, SH_AP_BATT, &right_x, floor_x, label, fg,
			       KT_A_NONE);
		}

		if (vol >= 0) {
			if (muted)
				snprintf(label, sizeof(label), "VOL off");
			else
				snprintf(label, sizeof(label), "VOL %d%%", vol);
			applet(sh, SH_AP_VOL, &right_x, floor_x, label,
			       muted ? KT_DIM : KT_TEXT, KT_A_NONE);
		}

		snprintf(label, sizeof(label), "NET %.12s", iface);
		applet(sh, SH_AP_NET, &right_x, floor_x, label,
		       up ? KT_TEXT : KT_DIM, KT_A_NONE);

		if (panel_restarts > 0) {
			/* A mark rather than a word: the panel is one row, and
			 * the point is to be noticed and clicked. */
			snprintf(label, sizeof(label), "%s",
				 ktui_glyph[KT_G_BULLET]);
			applet(sh, SH_AP_RESTART, &right_x, floor_x, label,
			       KT_WARN, KT_A_NONE);
		}

		/*
		 * ── the recording indicators, left of everything else ──
		 *
		 * The one thing on this panel that is a WARNING rather than a
		 * fact. The microphone takes the secondary colour; the camera
		 * and a screen share take reverse video as well — being seen
		 * and being watched outrank being heard. One app is named and
		 * the rest are counted: the panel is one row.
		 */
		static const char *const priv_word[] = { "MIC", "CAM", "SCR" };
		for (int kind = SH_PRIV_MIC; kind < SH_PRIV_NKIND; kind++) {
			int n = sh_priv_count(sh, kind);
			const char *who = n > 0 ? sh_priv_name(sh, kind) : NULL;
			char nm[SH_PRIV_NAME];

			if (n <= 0)
				continue;
			/* Cut by CODEPOINT: this is `application.name`, the
			 * app's own string in whatever script it likes, and a
			 * byte cut leaves half a sequence in the label for
			 * ktui_utf8_width to measure. */
			sh_utf8_trunc(nm, sizeof(nm), who ? who : "?",
				      n > 1 ? 14 : 20);
			if (n > 1)
				snprintf(label, sizeof(label), "%s%s %s +%d",
					 ktui_glyph[KT_G_BULLET],
					 priv_word[kind], nm, n - 1);
			else
				snprintf(label, sizeof(label), "%s%s %s",
					 ktui_glyph[KT_G_BULLET],
					 priv_word[kind], nm);

			int lw = ktui_utf8_width(label);
			if (right_x - floor_x < lw + 2)
				break;
			right_x -= lw + 1;
			/* The emphasis is a FILL plus swapped slots, not
			 * KT_A_REVERSE: the attribute inverts only the cells
			 * the text covers, so `●CAM firefox` would come out as
			 * two lit blocks with an unlit gap. */
			if (kind != SH_PRIV_MIC) {
				ktui_draw_fill(krect(right_x, 0, lw, 1),
					       KT_WARN);
				ktui_draw_text(right_x, 0, lw, label,
					       KT_SURFACE, KT_WARN, KT_A_NONE);
			} else {
				ktui_draw_text(right_x, 0, lw, label, KT_WARN,
					       KT_SURFACE, KT_A_NONE);
			}
		}

		/*
		 * ── the tray, immediately left of the clock ──
		 *
		 * One cell per item and no icon: this is a character grid, and
		 * the first letter of an item's Id is what survives the
		 * translation. Status is the colour — passive dim, active in
		 * the text colour, NeedsAttention in the accent AND reversed.
		 */
		int ntray = sh_tray_count(sh);
		if (ntray > 0 && right_x - floor_x > ntray * 2 + 4) {
			int tx = right_x - ntray * 2 - 1;
			sh->tray_hit_x = tx;
			for (int i = 0; i < ntray; i++) {
				const struct sh_tray_item *it =
					sh_tray_get(sh, i);
				char cell[8];
				/*
				 * An item whose Id has not been read yet gets a
				 * dim placeholder, never a letter mined from its
				 * bus address: every Qt item registers as
				 * org.kde.StatusNotifierItem-<pid>-<n>, so a
				 * KDE-ish login drew a row of identical 'O's
				 * until the properties arrived.
				 */
				if (!it->id[0]) {
					ktui_draw_text(tx, 0, 1,
						       ktui_glyph[KT_G_DOT],
						       KT_DIM, KT_SURFACE,
						       KT_A_NONE);
					tx += 2;
					continue;
				}
				/* One BYTE, not one codepoint: ids are program
				 * names — ascii in every case that exists — so
				 * the fallback is a dot, not a decoder. */
				snprintf(cell, sizeof(cell), "%c",
					 (unsigned char)it->id[0] < 0x80
						 ? it->id[0] : '.');
				if (cell[0] >= 'a' && cell[0] <= 'z')
					cell[0] = (char)(cell[0] - 'a' + 'A');
				int fg = it->status == SH_TRAY_ATTENTION
						 ? KT_ACCENT
					 : it->status == SH_TRAY_ACTIVE
						 ? KT_TEXT
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

		/* ── middle: the chips, in whatever room is left ──
		 *
		 * The -1 is the gap before whatever the right wing put there,
		 * and draw_chips is given the same bound the decision below
		 * uses: passing `right_x` instead let a `+2` end up against the
		 * first digit of the clock.
		 */
		int avail = right_x - x - 1;
		/* Collapse the menu bar before dropping a single chip. */
		if (!compact && nchips > 0 && avail < nchips * 4)
			continue;
		draw_chips(sh, x, x + avail, 0);
		break;
	}

	ktui_draw_flush();
}


/* ── the bottom panel ──────────────────────────────────────────────────────
 *
 *   mc  GIMP │ [□ foot ×2] [■ GIMP]                          ▪▫▫▫    ░
 *
 * GNOME 2's second panel: pinned favorites and the window list on the left,
 * the workspace pager and show-desktop on the right. A SEPARATE FUNCTION
 * rather than a mode inside draw_panel(), because the top panel's layout is
 * load-bearing — it lays out right to left so the clock cannot be pushed off
 * a narrow screen — and threading a second set of applets through it would
 * put both at risk to change either.
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

	clear_hits(sh);
	if (w < 20 || h < 1)
		return;

	build_chips(sh);
	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);

	/* Right first, for the same reason the top panel does it: the pager has
	 * a fixed width and the window list is what has to give. */
	int show_x = w - 2;
	ktui_draw_text(show_x, 0, 1, ktui_glyph[KT_G_SHADE], KT_DIM, KT_SURFACE,
		       KT_A_NONE);
	sh->show_hit_x = show_x;
	sh->show_hit_end = show_x + 1;

	/*
	 * The pager, CLAMPED: sixteen workspaces on a narrow output pushed
	 * pager_x negative, the recorded span covered the window list, and a
	 * click on an empty stretch of panel switched workspaces. When the
	 * squares do not fit, a compact `N/M` readout is drawn instead and the
	 * pager records NO span — only what was actually drawn may be hit.
	 */
	int pager_w = sh->nws * 2;
	int pager_x = show_x - pager_w - 3;
	int list_limit;
	if (sh->nws > 0 && pager_x >= 8) {
		sh->pager_hit_x = pager_x;
		for (int i = 0; i < sh->nws; i++) {
			/*
			 * A filled square for the active workspace, a hollow
			 * one for the rest — a pager is aimed at, not read.
			 * Urgency is the warning colour here too.
			 */
			bool active = i == sh->active_ws;
			ktui_draw_text(pager_x + i * 2, 0, 1,
				       active ? ktui_glyph[KT_G_SQUARE]
					      : ktui_glyph[KT_G_DOT],
				       active ? KT_ACCENT
				       : sh->ws_urgent[i] ? KT_WARN
							  : KT_DIM,
				       KT_SURFACE, KT_A_NONE);
		}
		sh->pager_hit_end = pager_x + pager_w;
		list_limit = pager_x - 1;
	} else if (sh->nws > 0) {
		char ro[32];
		snprintf(ro, sizeof(ro), "%d/%d", sh->active_ws + 1, sh->nws);
		int rw = ktui_utf8_width(ro);
		int rx = show_x - rw - 2;
		if (rx < 1)
			rx = 1;
		ktui_draw_text(rx, 0, rw, ro, KT_DIM, KT_SURFACE, KT_A_NONE);
		list_limit = rx - 1;
	} else {
		list_limit = show_x - 2;
	}

	/* ── the favorites row, before the window list ── */
	int x = 1;
	int drew_fav = 0;
	for (int i = 0; i < nfavs; i++) {
		int lw = ktui_utf8_width(favs[i].name);
		if (lw > 10)
			lw = 10;
		if (x + lw + 4 >= list_limit)
			break;	/* spans past here stay empty: not drawn */
		fav_x[i] = x;
		x += ktui_draw_text(x, 0, lw, favs[i].name, KT_MID, KT_SURFACE,
				    KT_A_NONE);
		fav_end[i] = x;
		x += 2;
		drew_fav = 1;
	}
	if (drew_fav) {
		x += ktui_draw_text(x, 0, 1, ktui_glyph[KT_G_VL], KT_DIM,
				    KT_SURFACE, KT_A_NONE);
		x += 1;
	}

	/* ── the window list ── */
	draw_chips(sh, x, list_limit, 1);
	ktui_draw_flush();
}

/* Clicks on the bottom panel. Kept beside its drawing for the same reason the
 * two are split at all: the hit map is what the last frame actually drew. */
static void handle_click_bottom(struct sh_state *sh, int cx, int btn)
{
	int ci = chip_at(sh, cx);
	if (ci >= 0) {
		chip_click(sh, ci, btn, 1);
		return;
	}
	if (btn != SH_TRAY_BTN_LEFT)
		return;

	if (plusn_end > plusn_x && cx >= plusn_x && cx < plusn_end) {
		/* Step the window; the wheel does the same with less aim. */
		chip_off = chip_off >= nchips - chip_vis ? 0 : chip_off + 1;
		return;
	}
	for (int i = 0; i < nfavs; i++) {
		if (fav_end[i] > fav_x[i] && cx >= fav_x[i] &&
		    cx < fav_end[i]) {
			fav_launch(i);
			return;
		}
	}
	/* A half-open span like every other hit test here, and this is the one
	 * that has to be: the glyph is one cell at w - 2, and an open-ended
	 * comparison made the blank last column minimise every window. */
	if (sh->show_hit_end > sh->show_hit_x && cx >= sh->show_hit_x &&
	    cx < sh->show_hit_end) {
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
	if (sh->pager_hit_end > sh->pager_hit_x && cx >= sh->pager_hit_x &&
	    cx < sh->pager_hit_end) {
		int i = (cx - sh->pager_hit_x) / 2;
		if (i >= 0 && i < sh->nws)
			sh_activate_workspace(sh, i);
	}
}

/* ── clicks ────────────────────────────────────────────────────────────── */

/*
 * Which workspace digit is at this cell, or -1.
 *
 * Reads the positions the last frame RECORDED rather than dividing by a
 * stride: labels are one to eight cells wide (digits, then names), and only
 * the draw knows where each one landed.
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
		if (sh->menu_hit_end[i] > sh->menu_hit_x[i] &&
		    cx >= sh->menu_hit_x[i] && cx < sh->menu_hit_end[i])
			return i;
	return -1;
}

/* The pointer moved. Nothing here acts; it only records what is under it, so
 * the next frame can light it. The bottom panel has a window list too and no
 * menu bar, which is the only difference. */
static void handle_motion(struct sh_state *sh, int cx, int is_bottom)
{
	sh->hover_menu = is_bottom ? -1 : menu_at(sh, cx);
	sh->hover_task = chip_at(sh, cx);
}

/*
 * The wheel: volume over the volume applet, SNI Scroll over a tray item (a
 * volume tray icon expects exactly that), workspace stepping over the strip,
 * and shift-or-cycle over the window list.
 */
static void handle_wheel(struct sh_state *sh, int cx, int up)
{
	if (applet_at(sh, cx) == SH_AP_VOL) {
		int muted = 0, vol = sh_volume_get(&muted);
		if (vol >= 0)
			sh_volume_set(vol + (up ? 5 : -5));
		return;
	}
	if (sh->tray_hit_end > sh->tray_hit_x && cx >= sh->tray_hit_x &&
	    cx < sh->tray_hit_end) {
		sh_tray_scroll(sh, (cx - sh->tray_hit_x) / 2, up ? 1 : -1);
		return;
	}
	if (list_x1 > list_x0 && cx >= list_x0 && cx < list_x1) {
		chips_wheel(sh, up);
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

static void handle_wheel_bottom(struct sh_state *sh, int cx, int up)
{
	if (list_x1 > list_x0 && cx >= list_x0 && cx < list_x1)
		chips_wheel(sh, up);
}

/*
 * A click on the right wing.
 *
 * Each applet does the one thing a person aiming at it wants: the volume mutes,
 * the network opens the tool that can change it, the clock shows a calendar,
 * the restart mark explains itself. LEFT only — a middle or right click on a
 * readout is a miss, not a request, and spawning nmtui on one taught people to
 * fear the panel's right half.
 */
static void handle_applet(struct sh_state *sh, int id, int btn)
{
	if (btn != SH_TRAY_BTN_LEFT)
		return;
	switch (id) {
	case SH_AP_VOL:
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
		snprintf(xs, sizeof(xs), "%d", sh->ap_x[id] * kwl_cell_w());
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

	int ci = chip_at(sh, cx);
	if (ci >= 0) {
		chip_click(sh, ci, btn, 0);
		return;
	}

	if (btn != SH_TRAY_BTN_LEFT)
		return;

	if (plusn_end > plusn_x && cx >= plusn_x && cx < plusn_end) {
		chip_off = chip_off >= nchips - chip_vis ? 0 : chip_off + 1;
		return;
	}
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

/* ── autohide ──────────────────────────────────────────────────────────────
 *
 * `--autohide`, from comp.conf's `panel_autohide` through kdos-child.
 *
 * The half that matters is libkwl's: hidden, the panel drops its exclusive
 * zone, so the strip it was holding goes back to the windows. What is left on
 * screen is one row of accent shade against the theme background — an edge
 * that is visibly THERE, because a panel that vanished completely is a panel
 * nobody finds again.
 *
 * The pointer is the whole interface. An enter over the strip shows the panel
 * at once; a leave arms a deadline rather than hiding immediately, or crossing
 * the bar on the way to a window's titlebar would snap it shut under the hand.
 * The deadline rides libkwl's poll timeout — the same plumbing key repeat uses
 * — so nothing polls on a timer to service it.
 */
#define AH_HIDE_MS 500

static int autohide;		/* --autohide was given                    */
static int ah_hidden;		/* the panel is collapsed right now        */
static int64_t ah_hide_at;	/* when to collapse it, or 0 for "not armed" */

static int64_t ah_now_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void ah_show(void)
{
	ah_hide_at = 0;
	if (!ah_hidden)
		return;
	ah_hidden = 0;
	kwl_layer_autohide(false);
	ktui_draw_invalidate();
}

static void ah_hide(void)
{
	ah_hide_at = 0;
	if (ah_hidden)
		return;
	ah_hidden = 1;
	kwl_layer_autohide(true);
	ktui_draw_invalidate();
}

/* The edge. One row of shade in the accent — the vt tier has ░, so this reads
 * the same on the console font as it does under fcft. */
static void ah_draw_edge(void)
{
	int w = ktui_w;

	ktui_draw_fill(krect(0, 0, w, ktui_h), KT_BG);
	ktui_draw_hline(0, 0, w, KT_G_SHADE, KT_ACCENT, KT_BG);
	ktui_draw_flush();
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
		/* comp.conf's `panel_autohide`, passed through by kdos-child.
		 * Per panel, so a machine can hide the window list and keep
		 * the clock. */
		else if (!strcmp(argv[i], "--autohide"))
			autohide = 1;
		else if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		/* strftime, straight through — the panel guards the call, not
		 * the format. kdos-child passes comp.conf's clock_format. */
		else if (!strcmp(argv[i], "--clock") && i + 1 < argc)
			clock_fmt = argv[++i];
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
				"usage: kdos-shell [--bottom] [--autohide] "
				"[--output NAME] [--font NAME] [--clock FMT]\n"
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
	if (is_bottom)
		load_favorites();

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
		/* Read the numbers, run no policy: a dump on a laptop below 3%
		 * used to suspend the machine it was diagnosing. */
		panel_measure();
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
	/* Start hidden: a panel that came up shown and then collapsed a
	 * moment later would read as a redraw fault at every login. */
	if (autohide)
		ah_hide();

	while (!kwl_should_close()) {
		if (sh_theme_dirty) {
			sh_theme_dirty = 0;
			sh_theme_from_cache();
			ktui_draw_invalidate();
		}
		/* Above the draw branch, not inside draw_panel: an autohidden
		 * panel draws the edge and nothing else, and the low-battery
		 * warning and the auto-suspend must not be a side effect of
		 * whether the pointer happens to be on the bar. The top panel
		 * only — the bottom one draws neither readout, and two
		 * processes warning about one battery is one warning too many. */
		if (!is_bottom)
			panel_tick(&sh);
		if (autohide && ah_hidden)
			ah_draw_edge();
		else if (is_bottom)
			draw_bottom(&sh);
		else
			draw_panel(&sh);

		KtuiEvent ev;
		/*
		 * A second, not a frame rate. The panel's fastest-changing thing
		 * is a clock that ticks once a minute; redrawing at display rate
		 * would burn a core to show the same pixels. Any event — a
		 * window appearing, a click — wakes it sooner.
		 *
		 * An armed autohide deadline shortens the wait to itself, the
		 * way libkwl already shortens it for a held key: without that
		 * the panel would stay up for whatever the poll happened to
		 * be, which here is a full second past the leave.
		 */
		int wait = 1000;
		if (autohide && ah_hide_at) {
			int64_t rem = ah_hide_at - ah_now_ms();
			if (rem < 0)
				rem = 0;
			if (rem < wait)
				wait = (int)rem;
		}
		if (ktui_backend()->poll_event(&ev, wait) &&
		    ev.type == KT_EVT_MOUSE) {
			/* Plain movement arrives as KT_MP_DRAG — libkwl's
			 * spelling, and the same one kdos-menu reads. An
			 * off-grid x is libkwl's pointer LEAVE. */
			if (autohide && ev.press == KT_MP_DRAG) {
				if (ev.mx >= 0)
					ah_show();
				else if (!ah_hidden && !ah_hide_at)
					ah_hide_at = ah_now_ms() + AH_HIDE_MS;
			}
			/* Nothing on the strip is a control: it is one row of
			 * shade, and the frame that drew it recorded no hit
			 * map. Clicks and the wheel wait for the panel. */
			if (autohide && ah_hidden) {
				/* the edge answers hover and nothing else */
			} else if (ev.press == KT_MP_DRAG) {
				handle_motion(&sh, ev.mx, is_bottom);
			} else if (ev.press == KT_MP_PRESS) {
				if (ev.btn == KT_MB_WHEEL_UP ||
				    ev.btn == KT_MB_WHEEL_DOWN) {
					(is_bottom ? handle_wheel_bottom
						   : handle_wheel)(&sh, ev.mx,
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
		/* The deadline is spent here rather than in the event branch:
		 * a leave with nothing following it produces no further event
		 * at all, and the hide has to happen anyway. */
		if (autohide && ah_hide_at && ah_now_ms() >= ah_hide_at)
			ah_hide();
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
