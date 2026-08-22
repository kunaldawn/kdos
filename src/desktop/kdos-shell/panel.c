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
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "kcell.h"
#include "kproc.h"
#include "kicon.h"
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

/*
 * The same, keeping the pid — because this child has to be KILLED later.
 *
 * panel_spawn double-forks precisely so nothing has to be reaped; a tooltip is
 * the one thing on this bar with a lifetime shorter than the pointer's, so it
 * is a single fork whose pid is kept and waited for when it is signalled.
 */
static pid_t panel_spawn_pid(const char *const argv[])
{
	pid_t pid = fork();

	if (pid == 0) {
		setsid();
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	return pid;
}

/*
 * WHICH EDGE THE BAR IS ON, because a popup belonging to it has to grow the
 * other way. Layer-shell has no coordinates: `--at-bottom` is an anchor to the
 * BOTTOM edge plus a margin, which is the only way a client can say "just
 * above the taskbar" without knowing the output's pixel height — and it is
 * exactly wrong for a bar on the top, where the same popup has to hang DOWN
 * from it. Every one of these spawns passed `--at-bottom` unconditionally, so
 * `kdos-shell --top` put its own menus at the far end of the screen.
 */
static int panel_top;

static const char *panel_at_flag(void)
{
	return panel_top ? "--at" : "--at-bottom";
}

/* The menu the Start button opened, while it is still up. */
/*
 * The Start button's id in the popup record. Negative so it can never collide
 * with an applet's, which is what every other entry there is keyed on.
 */
#define POPUP_START (-2)

/*
 * ── A PANEL BUTTON TOGGLES ITS POPUP ───────────────────────────────────────
 *
 * Clicking the clock opened a calendar; clicking the clock again opened
 * ANOTHER one. Every button on this bar behaved that way, because a popup is a
 * separate PROCESS here and the panel spawned one per click without ever
 * asking whether the last one was still up. That is not what a button that
 * opens a panel does anywhere else, and it is the reason a hand that wants the
 * calendar shut has to go looking for Escape.
 *
 * ONE POPUP AT A TIME, because that is what the surfaces themselves already
 * assume: they take the keyboard on demand and close when they lose it, so two
 * of them on screen is a state neither can be in for long. The panel keeps the
 * pid and WHICH control opened it; a click on that same control while it is up
 * kills it and opens nothing.
 *
 * The pid, not the watched pipe: the Start menu's fd answers "is it still
 * there" and cannot CLOSE it, and closing it is the whole point. It is a
 * single fork, so it is also the one thing on this bar that has to be reaped —
 * the same trade the tooltip already makes.
 */
static pid_t popup_pid = -1;
static int popup_which = -1;	/* the control that opened it, or -1 */

/* Reap it if it has gone. Called once a frame and before every decision. */
static int popup_alive(void)
{
	if (popup_pid <= 0)
		return 0;
	if (waitpid(popup_pid, NULL, WNOHANG) == popup_pid) {
		popup_pid = -1;
		popup_which = -1;
		return 0;
	}
	return 1;
}

static void popup_close(void)
{
	if (popup_pid > 0) {
		kill(popup_pid, SIGTERM);
		waitpid(popup_pid, NULL, 0);
	}
	popup_pid = -1;
	popup_which = -1;
}

/*
 * Open `argv` as THIS control's popup, or — when it is already this control's
 * popup that is up — close it and open nothing.
 *
 * Returns 1 when it toggled something shut, so a caller that has other work to
 * do on a click can tell the two apart.
 *
 * A DIFFERENT control always replaces: clicking the clock while the volume
 * slider is up is a request for the calendar, not for two popups or for
 * nothing.
 */
static int popup_toggle(int which, const char *const argv[])
{
	int up = popup_alive();

	if (up && popup_which == which) {
		popup_close();
		return 1;
	}
	if (up)
		popup_close();
	popup_pid = panel_spawn_pid(argv);
	popup_which = popup_pid > 0 ? which : -1;
	return 0;
}

/* The Start button stays lit for as long as its menu is up. */
static int start_menu_open(void)
{
	return popup_alive() && popup_which == POPUP_START;
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
static int net_state(char *out, size_t n, int *wireless)
{
	DIR *d = opendir("/sys/class/net");
	struct dirent *e;
	int up = 0;

	snprintf(out, n, "down");
	if (wireless)
		*wireless = 0;
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
			if (wireless)
				*wireless = wifi;
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
	/* The desktop-entry id, kept so Unpin knows what line to remove: the
	 * file is a list of ids and the label is not one. */
	char id[128];
	char name[24];
	char exec[256];
	/* The entry's own `Icon=`, resolved once at load. Quick launch is a row
	 * of pictures when there is artwork and a row of names when there is
	 * not — the same fallback every other surface keeps. */
	char icon[64];
};
/*
 * A CONTROL THAT DOES NOT ANSWER THE POINTER IS NOT A CONTROL.
 *
 * The quick-launch row had no hover state at all: the icons were pictures on
 * a bar and nothing about them said they could be clicked. `hover_fav` is
 * what the last motion landed on.
 *
 * `fav_anim` is the LAUNCH: a click starts something that takes seconds to
 * appear, and a launcher that looks identical before and after the click is
 * one people click twice. The icon pulses for a beat and a bit, which is long
 * enough to be seen and short enough not to outlast a warm start. The panel
 * shortens its own poll while it runs — see the wait computation in the loop —
 * so an idle bar still wakes once a second and nothing here costs a frame
 * clock.
 */
#define FAV_ANIM_MS 1100
static int64_t panel_now_ms(void);	/* the meters' clock, shared */
static int hover_fav = -1;
/*
 * A DRAG ON THE QUICK-LAUNCH ROW REORDERS IT.
 *
 * The order of that row is the one thing about it a person can have an
 * opinion about, and the only way to change it was to edit
 * `~/.config/kdos/favorites` by hand. Wayland delivers plain motion and
 * dragged motion identically — a wl_pointer.motion event carries no button
 * state — so the BUTTON is what has to be remembered, exactly as the volume
 * slider remembers it: a press on an icon arms a drag, motion tracks which
 * icon it is over, and the RELEASE decides. Which is also why a launch moved
 * from the press to the release; that is what a draggable icon does
 * everywhere, and a launch on press would fire before the drag could begin.
 */
static int drag_fav = -1;	/* the icon a press armed, or -1 */
static int drag_over = -1;	/* where it would land right now  */
static int drag_moved;		/* the pointer has left its cell  */
static int fav_anim = -1;
static int64_t fav_anim_at;

static struct fav favs[FAV_MAX];
static int nfavs;
static int fav_x[FAV_MAX], fav_end[FAV_MAX];

/* When the favorites file was last read, so a pin from a menu in another
 * process shows up without a re-login. */
static long favs_mtime;

/* Where the pinned list lives. One answer, because a writer and a reader that
 * disagreed about the path would be a pin that never appears. */
static int favorites_path(char *out, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(out, n, "%s/kdos/favorites", cfg);
	else if (home && *home)
		snprintf(out, n, "%s/.config/kdos/favorites", home);
	else
		return -1;
	return 0;
}

static void load_favorites(void)
{
	char path[512], line[256];
	FILE *f;
	struct stat st;

	nfavs = 0;
	if (favorites_path(path, sizeof(path)) != 0)
		return;
	favs_mtime = stat(path, &st) == 0 ? (long)st.st_mtime : 0;
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
		snprintf(fv->id, sizeof(fv->id), "%s", s);
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
		const char *ic = kicon_app_icon(s);
		snprintf(fv->icon, sizeof(fv->icon), "%s", ic ? ic : s);
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
	if (n) {
		fav_anim = i;
		fav_anim_at = panel_now_ms();
		panel_spawn(argv);
	}
}

/* Set by panel_main from `--cells`; comp.conf's `panel_cells`. */
static int tb_rows = 2;
/*
 * Whether a window button carries its name — `task_labels` in panel.conf.
 * AUTO is the adaptive ladder the bar has always used; ALWAYS keeps the label
 * even when it means hiding windows behind `+N`; NEVER is a dock.
 */
enum { TL_AUTO = 0, TL_ALWAYS, TL_NEVER };
static int task_labels = TL_AUTO;

/*
 * WHICH METERS EXIST. The identity is here and the descriptor table is beside
 * the series it points at, several hundred lines down — panel.conf is parsed
 * long before any of them are sampled, and a table that had to be hoisted with
 * its enum would drag the sampling half up with it.
 */
enum { MT_CPU = 0, MT_RAM, MT_DISK, MT_NET, MT_DIO, MT_N };
static int meters_sel[MT_N] = { MT_CPU, MT_RAM, MT_NET };
static int nmeters_sel = 3;
static int meter_by_key(const char *name);

/* `icons = off` in comp.conf, or `--no-icons`. Declared here rather than
 * beside the taskbar block below because the chip row consults it: whether a
 * crowded row may fall back to pictures is exactly this question. */
static int icons_on = 1;

static int icon_ok(void)
{
	return icons_on && task_labels != TL_ALWAYS;
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
/* What a window button needs in ICON MODE — the picture, the state marker and
 * a column of air. It is the floor the RIGHT WING measures against, because
 * the list gives up its labels before the bar gives up a chart. */
#define CHIP_MINW_ICON 3

/* Whether the row may collapse to pictures — `task_labels = always` is
 * somebody saying it may not, and a bar with no artwork cannot. Asked in two
 * places (the floor and the mode itself), so it is one function. */
static int icon_ok(void);

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
 * The task button's picture: the desktop entry's own icon when there is one.
 *
 * TWO CELLS WIDE and as tall as the bar, because a cell here is 16x32 and an
 * icon is square — libkicon centres the largest square that fits, so two cells
 * of a two-row taskbar is a 32x32 picture with 16 pixels of air above and
 * below it, which is exactly what a button wants. Minus one is a button with
 * no picture and a wider label, not a hole.
 *
 * The lookup is by `Icon=` first and by app_id second: `firefox-esr.desktop`
 * says `Icon=firefox-esr`, but plenty of entries name something else entirely,
 * and the entry is the thing that knows.
 */
static int chip_icon(const struct sh_state *sh, const struct chip *c, int h)
{
	const struct sh_task *t = &sh->tasks[c->first];
	const char *name;
	int s;

	(void)h;
	if (!t->app_id[0])
		return -1;
	/* 2x1 — a 32x32 square on the label's own row. See draw_start. */
	name = kicon_app_icon(t->app_id);
	s = name ? kicon_slot(name, 2, 1) : -1;
	if (s < 0)
		s = kicon_slot(t->app_id, 2, 1);
	return s;
}

/*
 * Draw the chip row into [x, limit), `h` cells tall. `marker` asks for the
 * □/■ state glyph; a button that carries an icon spends those cells on the
 * icon instead, which says the same thing in a way a person reads faster.
 * Equal widths, so clicking position N always means chip N — a proportional
 * layout would need the click map rebuilt whenever a browser tab changed a
 * title.
 *
 * NEVER zero chips while there are windows: when they do not all fit, the
 * spare go behind a `+N` cell and the wheel (or a click on the cell) shifts
 * the window. That is the contract the old per-window list broke.
 */
static void draw_chips(struct sh_state *sh, int x, int limit, int marker, int h)
{
	int avail = limit - x;
	int minw = marker ? CHIP_MINW_BOTTOM : CHIP_MINW_TOP;
	int ry = (h - 1) / 2;
	char pn[16];
	int pw;

	sh->task_hit_x = x;
	sh->task_cell_w = 0;
	chip_vis = 0;
	plusn_x = plusn_end = 0;
	list_x0 = list_x1 = 0;
	/*
	 * THE FLOOR HERE MUST BE THE ONE THE RIGHT WING MEASURED AGAINST.
	 *
	 * It was the LABEL floor while the wing had started reserving the ICON
	 * one, so a bar with five cells left for the window list passed the
	 * pass's acceptance test and then this returned without drawing — three
	 * foot windows open and a taskbar with nothing on it, photographed on
	 * the booted ISO. Two floors for one decision is one floor too many:
	 * this refuses only when there is not room for a single ICON chip, and
	 * the row is allowed to be one button wide.
	 */
	int floor_w = icon_ok() ? CHIP_MINW_ICON : minw + 1;

	if (nchips <= 0 || avail < floor_w)
		return;

	/* The overflow cell's REAL width, reserved before the chips divide the
	 * rest and measured at its widest (+N with everything but one hidden):
	 * a hardcoded three cells left `+12` drawn as a bare `+`, which says
	 * there are windows behind the cell and not how many. */
	snprintf(pn, sizeof(pn), "+%d", nchips - 1);
	pw = (int)strlen(pn);

	/*
	 * ICON MODE — the step every taskbar takes before it starts hiding
	 * windows, and the one this row did not have.
	 *
	 * The degradation used to be: full labels, then labels squeezed to the
	 * six-cell floor, then straight to a `+N` cell with windows behind it.
	 * On the shipped 80-column bar that meant a seventh window pushed one
	 * out of sight while the row still had space for a picture of it —
	 * which is the wrong trade, because a taskbar's entire job is to show
	 * you what is open. Windows 7, KDE and XFCE all drop the TEXT first
	 * and keep every button; so does this now.
	 *
	 * Three cells per window: two for the icon and one of air. A chip with
	 * no picture keeps the first letter of its name, so the row never
	 * contains a button that says nothing.
	 */
	/*
	 * `task_labels` decides, and AUTO is the ladder. NEVER is a dock and
	 * asks for icon mode whatever the room; ALWAYS refuses it and lets the
	 * `+N` cell carry the overflow instead, which is the trade somebody
	 * who set it has already made.
	 */
	int icon_mode = icon_ok() &&
			(task_labels == TL_NEVER ||
			 (task_labels == TL_AUTO && nchips * minw > avail)) &&
			nchips * 3 <= avail;
	if (icon_mode)
		minw = 3;

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
	/* Equal widths are what makes "the Nth cell is chip N" true, and in
	 * icon mode the whole point is that they are narrow — so the cap is
	 * the mode's own, not the label row's. */
	if (icon_mode && per > 4)
		per = 4;
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
		/*
		 * A WINDOW BUTTON IS A BUTTON, and it did not look like one.
		 *
		 * An inactive chip was filled with KT_SURFACE — the panel's
		 * own background — so it had no shape at all: the row read as
		 * a line of floating words with no edges, which is what "the
		 * task items need proper borders" is. KT_DIM is the palette's
		 * FILL colour (`dim`, 0x12401f against a 0x04120a panel) and
		 * is exactly what a raised tile wants; the label on it is
		 * KT_TEXT at better than 4:1, so nothing is traded for the
		 * shape. Active keeps the accent and the swapped slots.
		 *
		 * FILL, then draw with the slots swapped — not KT_A_REVERSE
		 * over the label. The attribute inverts the cells the TEXT
		 * occupies, so the spaces inside a name were left at the
		 * panel's own background and the focused entry came out as one
		 * highlighted block per WORD: `▓GNU▓ ▓Image▓ ▓Mani▓`.
		 */
		int hovered = sh->hover_task == chip_off + k;
		int fg, bg;
		if (c->active) {
			fg = KT_SURFACE;
			bg = KT_ACCENT;
		} else if (hovered) {
			/* Brighter than at rest and dimmer than focused, so
			 * hover cannot be misread as "this is the window you
			 * are in": an affordance, not a state. */
			fg = KT_SURFACE;
			bg = KT_MID;
		} else {
			fg = c->allmin ? KT_MID : KT_TEXT;
			bg = KT_DIM;
		}
		ktui_draw_fill(krect(x, 0, per - 1, h), bg);

		int icon = (icon_mode || per >= 6) ? chip_icon(sh, c, h) : -1;
		/* Where the label starts: after the picture, after the state
		 * marker, or at the edge. The sub-line below has to begin in
		 * the same column or the button reads as two unrelated pieces
		 * of text stacked on each other. */
		int label_x = icon >= 0 ? 3 : marker ? 2 : 0;

		/*
		 * ICON MODE: the picture, vertically centred over BOTH rows
		 * because there is no label to line up with, and a one-cell
		 * state marker under it. A chip with no artwork keeps the
		 * first letter of its name — a blank button is worse than a
		 * narrow one.
		 */
		if (icon_mode) {
			int iy = h > 1 ? ry : 0;
			if (icon >= 0) {
				ktui_draw_sprite(krect(x, iy, 2, 1), icon, fg,
						 bg);
			} else {
				char one[8];
				snprintf(one, sizeof(one), "%.1s",
					 c->label && *c->label ? c->label : "?");
				ktui_draw_text(x, iy, 2, one, fg, bg,
					       KT_A_NONE);
			}
			if (h > 1)
				ktui_draw_text(x, ry + 1, 2,
					       c->allmin
						       ? ktui_glyph[KT_G_DOT]
						       : ktui_glyph[KT_G_SQUARE],
					       c->allmin ? KT_DIM : fg, bg,
					       KT_A_NONE);
			x += per;
			continue;
		}

		/*
		 * THE SECOND ROW IS THE SUB-LINE.
		 *
		 * On a two-row bar the button used to be a two-row block of
		 * colour with a single row of text at the top of it, and that
		 * is most of why this bar read as unaligned. It carries the
		 * window's own TITLE under the application's name now —
		 * `Firefox` over `KDOS — Mozilla Firefox` — which is precisely
		 * the half a one-row taskbar has to throw away, and for a
		 * group it says how many windows are behind the button.
		 */
		if (h > 1 && per - label_x > 3) {
			const struct sh_task *t0 = &sh->tasks[c->first];
			char sub[96];
			if (c->count > 1)
				snprintf(sub, sizeof(sub), "%d windows",
					 c->count);
			else
				snprintf(sub, sizeof(sub), "%s", t0->title);
			/* Case-INSENSITIVE: foot's toplevel is titled `foot`
			 * and its entry is named `Foot`, so a plain strcmp
			 * drew the same word twice, once under the other. */
			if (sub[0] && strcasecmp(sub, c->label))
				/* KT_MID, not KT_DIM: the button IS KT_DIM
				 * now, and a sub-line in the same colour as
				 * the thing it is drawn on is invisible. */
				ktui_draw_text(x + label_x, ry + 1,
					       per - 1 - label_x, sub,
					       c->active ? fg : KT_MID, bg,
					       KT_A_NONE);
		}

		if (icon >= 0) {
			ktui_draw_sprite(krect(x, ry, 2, 1), icon, fg, bg);
			ktui_draw_text(x + 3, ry, per - 4, c->label, fg, bg,
				       KT_A_NONE);
		} else if (marker) {
			/* A filled square is a window you can see, a hollow
			 * one a group entirely minimised — italics and grey
			 * do not survive eight colours and one weight. */
			ktui_draw_text(x, ry, 1,
				       c->allmin ? ktui_glyph[KT_G_DOT]
						 : ktui_glyph[KT_G_SQUARE],
				       c->allmin ? KT_DIM : fg, bg, KT_A_NONE);
			ktui_draw_text(x + 2, ry, per - 3, c->label, fg, bg,
				       KT_A_NONE);
		} else {
			ktui_draw_text(x, ry, per - 1, c->label, fg, bg,
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
			x += ktui_draw_text(x, ry, limit - x, pn, KT_WARN,
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
static void spawn_windows_menu(struct sh_state *sh, int ci, int ctrl)
{
	char xs[16], ys[16];
	const char *app_id = sh->tasks[chips[ci].first].app_id;

	snprintf(xs, sizeof(xs), "%d",
		 (sh->task_hit_x + (ci - chip_off) * sh->task_cell_w) *
			 kwl_cell_w());
	snprintf(ys, sizeof(ys), "%d", kwl_px_h());
	const char *argv[] = { "kdos-menu", ctrl ? "--winmenu" : "--windows",
			       app_id, panel_at_flag(), xs, ys, NULL };
	panel_spawn(argv);
}

/*
 * A click on a chip.
 *
 * LEFT keeps the semantics every taskbar has had since Windows 95: it toggles
 * a single window (minimise the one you are in, restore the one you are not)
 * and opens the member list for a group. MIDDLE closes politely, so an editor
 * with unsaved work still gets to ask.
 *
 * RIGHT OPENS THE WINDOW MENU, and that is the change. It used to MINIMISE —
 * which is a second way to do what left-click already does, on the button
 * every other desktop reserves for Restore/Maximize/Close. There was no way at
 * all to maximise or restore a window from this bar, and on a system where
 * most windows belong to boxed applications that is the bar you are holding.
 * See kdos-menu's `--winmenu`.
 */
static void chip_click(struct sh_state *sh, int ci, int btn)
{
	if (ci < 0 || ci >= nchips)
		return;
	const struct chip *c = &chips[ci];

	if (btn == SH_TRAY_BTN_RIGHT) {
		spawn_windows_menu(sh, ci, 1);
		return;
	}
	if (c->count == 1) {
		if (btn == SH_TRAY_BTN_MIDDLE)
			sh_close_task(sh, c->first);
		else
			sh_toggle_task(sh, c->first);
		return;
	}
	/* A group's middle click does nothing rather than closing every window
	 * of an application at once: the member menu carries Close all, where
	 * the row says how many. */
	if (btn == SH_TRAY_BTN_LEFT)
		spawn_windows_menu(sh, ci, 0);
}

/* ── drawing ───────────────────────────────────────────────────────────── */

/*
 * Every span the click handlers read, emptied. Called at the TOP of both draw
 * paths, so an early return (a surface squeezed under 20 columns during a
 * resize) cannot leave the previous frame's hit map live — which is exactly
 * the "hit map outlives what it describes" defect the applet comment warns
 * about, shipped by the early return itself.
 */
/* The rows the meters strip covered last frame — one for the glyph fallback,
 * `rows` for the tile. Recorded from what was DRAWN, like every other span. */
static int meter_row0 = -1, meter_rows;
/*
 * WHAT THE POINTER IS OVER, for the half of the bar that had no answer.
 *
 * The Start button, the window buttons and the quick-launch row all lit under
 * the hand; the notification area did not — so the right third of this panel
 * was a dozen controls that looked like a readout, and the only way to find
 * out that the clock opens a calendar was to click the clock. Every one of
 * these is a FILL behind what is drawn, never an attribute over the label: the
 * rule the whole desktop keeps, because KT_A_REVERSE lights the cells a glyph
 * covers and leaves the spaces between words dark.
 */
static int hover_ap = -1;	/* a notification-area applet */
static int hover_ws = -1;	/* a workspace square         */
static int hover_tray = -1;	/* a tray item                */
static int hover_show;		/* the show-desktop column    */
/* The pointer is over it: the plot backdrop lifts, so a chart looks like the
 * control it is. Part of the tile's content hash, or nothing is redrawn. */
static int hover_meters;

static void clear_hits(struct sh_state *sh)
{
	meter_row0 = -1;
	meter_rows = 0;
	for (int i = 0; i < SH_NMENUS; i++)
		sh->menu_hit_x[i] = sh->menu_hit_end[i] = 0;
	sh->ws_hit_x = sh->ws_hit_end = 0;
	for (int i = 0; i < SH_MAX_WS; i++)
		sh->ws_hit[i] = -1;
	sh->pager_hit_x = sh->pager_hit_end = 0;
	sh->show_hit_x = sh->show_hit_end = -1;
	sh->task_hit_x = 0;
	sh->task_cell_w = 0;
	sh->start_x = sh->start_end = 0;
	sh->tray_hit_x = sh->tray_hit_end = 0;
	sh->meter_hit_x = sh->meter_hit_end = 0;
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
static int applet_row;		/* which row of the bar the wing sits on */

static int applet(struct sh_state *sh, int id, int *right_x, int x_min,
		  const char *label, int fg, int attr)
{
	int lw = ktui_utf8_width(label);

	sh->ap_x[id] = sh->ap_end[id] = 0;
	if (!lw || *right_x - x_min < lw + 2)
		return 0;
	*right_x -= lw + 1;
	if (hover_ap == id)
		ktui_draw_fill(krect(*right_x, applet_row, lw, 1), KT_DIM);
	ktui_draw_text(*right_x, applet_row, lw, label, fg,
		       hover_ap == id ? KT_DIM : KT_SURFACE, attr);
	sh->ap_x[id] = *right_x;
	sh->ap_end[id] = *right_x + lw;
	return 1;
}

/*
 * The same, FILLED — the emphasis rule for a cell grid. KT_A_REVERSE inverts
 * only the cells the text covers, so `●CAM firefox` comes out as two lit
 * blocks with an unlit gap between them.
 */
static int applet_lit(struct sh_state *sh, int id, int *right_x, int x_min,
		      const char *label, int bg)
{
	int lw = ktui_utf8_width(label);

	sh->ap_x[id] = sh->ap_end[id] = 0;
	if (!lw || *right_x - x_min < lw + 2)
		return 0;
	*right_x -= lw + 1;
	ktui_draw_fill(krect(*right_x, applet_row, lw, 1), bg);
	ktui_draw_text(*right_x, applet_row, lw, label, KT_SURFACE, bg,
		       KT_A_NONE);
	sh->ap_x[id] = *right_x;
	sh->ap_end[id] = *right_x + lw;
	return 1;
}

/*
 * ── ONE APPLET ON BOTH ROWS: a picture, a headline, a detail line ──────────
 *
 * THE SECOND ROW WAS EMPTY AND THE BAR IS TWO ROWS TALL. Every readout on the
 * right wing was one row of text with thirty-two pixels of nothing under it
 * the width of half the screen — `NET eth0`, `VOL 45%`, four workspace dots —
 * while the clock beside them used both rows and looked, correctly, like the
 * only finished thing there. A status area that spends half its height on
 * nothing is a status area that could be saying twice as much.
 *
 * So an applet is a two-row TILE: an icon, a headline that answers the
 * question the widget exists for, and a detail line that says what it is
 * doing. `NET` with the interface's name and its current rate; `VOL` with the
 * number and a level bar under it; a pager whose workspaces are little screens
 * with their numbers under them.
 *
 * THE ICON IS A 2x2 BOX, and that is not the rule the rest of this bar keeps.
 * Everywhere else a sprite is 2x1 — 32x32, a square on the row its label is
 * on — because an icon centred across two rows beside a label that sits in one
 * of them lines up with nothing. Here the label IS two rows, so the picture is
 * centred against the pair: libkicon draws the largest square that fits and
 * centres it, so a 2x2 box gets the same 32x32 picture, vertically centred
 * against both lines instead of sitting on one of them.
 *
 * Everything else is the one-row applet's contract, unchanged: right to left,
 * refuse rather than overflow, and record the span that was DRAWN.
 */
#define AP_WIDE_TEXT 12

static int applet2(struct sh_state *sh, int id, int *right_x, int x_min,
		   const char *icon, const char *l1, const char *l2, int fg1,
		   int fg2, int bg)
{
	int w1 = l1 && *l1 ? ktui_utf8_width(l1) : 0;
	int w2 = l2 && *l2 ? ktui_utf8_width(l2) : 0;

	/* A one-row bar has no second line and no room for a picture beside a
	 * word; it is the same applet it always was, filled or not. */
	if (ktui_h < 2 || applet_row + 1 >= ktui_h)
		return bg != KT_SURFACE
			       ? applet_lit(sh, id, right_x, x_min, l1, bg)
			       : applet(sh, id, right_x, x_min, l1, fg1,
					KT_A_NONE);

	int slot = icons_on && icon ? kicon_slot(icon, 2, 2) : -1;
	int iw = slot >= 0 ? 3 : 0;
	/*
	 * A FIXED FIELD, for the same reason the compact tile has a fixed
	 * width: both labels here are somebody else's string — a track title, an
	 * application's own name — and they change while the panel is up. Sized
	 * to the widest applet on the bar rather than to the current word, the
	 * wing stops moving and the meters strip beside it stops sliding.
	 */
	int tw = AP_WIDE_TEXT;
	int width = iw + tw;

	if (w1 > tw)
		w1 = tw;
	if (w2 > tw)
		w2 = tw;

	sh->ap_x[id] = sh->ap_end[id] = 0;
	/* The field is fixed, so "nothing to say" has to be tested on the
	 * CONTENT: sizing to the widest label would otherwise draw an empty
	 * twelve-cell box for an applet with no text and no artwork. */
	if ((!w1 && !w2 && slot < 0) || *right_x - x_min < width + 2)
		return 0;
	*right_x -= width + 1;

	int x = *right_x;
	if (bg == KT_SURFACE && hover_ap == id)
		bg = KT_DIM;
	if (bg != KT_SURFACE)
		ktui_draw_fill(krect(x, applet_row, width, 2), bg);
	if (slot >= 0)
		ktui_draw_sprite(krect(x, applet_row, 2, 2), slot, fg1, bg);
	if (w1)
		ktui_draw_text(x + iw, applet_row, w1, l1, fg1, bg, KT_A_NONE);
	if (w2)
		ktui_draw_text(x + iw, applet_row + 1, w2, l2, fg2, bg,
			       KT_A_NONE);
	sh->ap_x[id] = x;
	sh->ap_end[id] = x + width;
	return 1;
}

/*
 * ── THE COMPACT FORM: A PICTURE WITH ITS NUMBER UNDER IT ───────────────────
 *
 * A NUMBER GOES UNDER ITS PICTURE; A NAME GOES BESIDE IT. The side-by-side
 * applet above is right for the things that carry a name — a track title, the
 * application holding the microphone — and it is far too wide for the ones
 * that carry a reading: `[icon] eth0 / ↑16` is eight cells to say what three
 * say, and on an eighty-column bar the wing ate forty-three of them and
 * squeezed the meters strip off the panel entirely the moment a window was
 * open. Measured, with KDOS_PANEL_DEBUG=1: `meters tile: no room
 * (right_x=35 x_min=33)`.
 *
 * Stacked, the same readout is three cells and looks like what a notification
 * area has looked like since XP: a row of small square pictures, each with its
 * value under it, all the same width. The interface's NAME is in the popup a
 * click opens — the icon already says wired or wireless, and the number
 * already says the link is alive.
 */
/*
 * THE WIDTH IS FIXED AND IT IS NOT A TASTE DECISION — a tile that sizes itself
 * to its own reading MOVES THE WHOLE BAR.
 *
 * The wing is laid out right to left and everything left of it — the meters
 * strip, the separator, the window list — starts where the walk stopped. So a
 * network readout that goes from `↓63` to `↓7` narrows the wing by a column and
 * every chart on the panel slides one cell sideways. Photographed four seconds
 * apart: `CPU` at column 24 and then at column 25, with the graphs apparently
 * scrolling backwards. Nothing was wrong with the charts.
 *
 * THREE CELLS, AND THE ODD NUMBER IS THE POINT. The icon is asked for as a
 * 3x1 sprite rather than 2x1: libkicon centres the largest square that fits,
 * so the picture is the same 32x32 and it is centred in the TILE rather than
 * in its left half. Three also centres the values that actually occur — a
 * count is one column and a rate or a level bar is three — where an even width
 * would leave every one of them half a cell off. That is the other half of
 * "the numbers are not aligned": with a two-cell tile and a one-character
 * value, `(width - vw) / 2` is zero, so the count sat under the icon's left
 * edge and read as belonging to the tray item beside it.
 */
#define AP_TILE_W 3

static int applet_tile(struct sh_state *sh, int id, int *right_x, int x_min,
		       const char *icon, const char *val, int fg1, int fg2)
{
	int vw = val && *val ? ktui_utf8_width(val) : 0;

	if (ktui_h < 2 || applet_row + 1 >= ktui_h)
		return applet(sh, id, right_x, x_min, val, fg1, KT_A_NONE);

	int width = AP_TILE_W;
	int slot = icons_on && icon ? kicon_slot(icon, width, 1) : -1;

	if (vw > width)
		vw = width;		/* clipped, never wider than the tile */

	sh->ap_x[id] = sh->ap_end[id] = 0;
	if (slot < 0 && !vw)
		return 0;
	if (*right_x - x_min < width + 2)
		return 0;
	*right_x -= width + 1;

	int x = *right_x;
	/* The affordance goes BEHIND the tile — the quick-launch row's rule,
	 * for the same reason: tinting the picture would change what the thing
	 * being pointed at looks like. */
	int bg = hover_ap == id ? KT_DIM : KT_SURFACE;
	if (bg != KT_SURFACE)
		ktui_draw_fill(krect(x, applet_row, width, 2), bg);
	if (slot >= 0) {
		ktui_draw_sprite(krect(x, applet_row, width, 1), slot, fg1, bg);
		if (vw)
			ktui_draw_text(x + (width - vw) / 2, applet_row + 1, vw,
				       val, fg2, bg, KT_A_NONE);
	} else if (vw) {
		/* No artwork: the value alone, on the row the rest of the wing
		 * uses. Half a tile is worse than the readout it replaced. */
		ktui_draw_text(x + (width - vw) / 2, applet_row, vw, val, fg1,
			       bg, KT_A_NONE);
	}
	sh->ap_x[id] = x;
	sh->ap_end[id] = x + width;
	return 1;
}

/* A bar of `cells` block glyphs, `pct` of them filled — the level under a
 * number, which is what makes `45%` read as a position rather than as a fact.
 * The rest is the shade glyph, so the track is visible at zero. */
static void level_bar(char *out, size_t n, int pct, int cells)
{
	/* Truncated, not rounded, with a floor of one for anything audible:
	 * ninety-one percent rounded up to four filled cells out of four is a
	 * bar that says "maximum" for most of its range. */
	int on = pct * cells / 100;

	if (!on && pct > 0)
		on = 1;

	out[0] = '\0';
	if (on < 0)
		on = 0;
	if (on > cells)
		on = cells;
	for (int i = 0; i < cells; i++) {
		char *p = out + strlen(out);
		size_t room = n - (size_t)(p - out);
		/* The empty part is a DOT, not a shade block: three shaded
		 * cells beside one lit one read as a second, dimmer bar rather
		 * than as the track behind the first. */
		snprintf(p, room, "%s",
			 ktui_glyph[i < on ? KT_G_FULL : KT_G_DOT]);
	}
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
static int panel_cpu = -1;
static int panel_clip = -1;
static int panel_media, panel_media_mounted;

/* Defined with the widget table below, measured here: a draw pass that
 * measures is a draw pass that ACTS, which is the rule panel_measure exists
 * to keep. */
static void meters_sample(void);
static int cpu_percent(void);
static int clip_depth(void);
static int media_count(int *mounted);
static void notify_poll(void);
static void frames_poll(void);

static void panel_measure(void)
{
	panel_pct = battery_percent(&panel_charging, &panel_discharging);
	panel_restarts = sh_restart_poll();
	/* Rate-limited inside: this is called on every loop turn and the loop
	 * is woken by events, not by a frame clock. See the meters block. */
	meters_sample();
	panel_cpu = cpu_percent();
	panel_clip = clip_depth();
	panel_media = media_count(&panel_media_mounted);
	notify_poll();
	frames_poll();
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

	/*
	 * A pin made from a MENU is made in another process, so the only way
	 * to know is to look. One stat a second against a file that is almost
	 * never touched, which is cheaper than any of the alternatives and is
	 * the same shape the theme watch uses.
	 */
	char fp[512];
	struct stat st;
	if (favorites_path(fp, sizeof(fp)) == 0) {
		long m = stat(fp, &st) == 0 ? (long)st.st_mtime : 0;
		if (m != favs_mtime) {
			load_favorites();
			hover_fav = -1;
			fav_anim = -1;
		}
	}
}

/* ── the notification area is a LIST, not a layout ─────────────────────────
 *
 * `~/.config/kdos/panel.conf`:
 *
 *     right = pager tray privacy mpris cpu restart net volume battery clock
 *
 * One line, one ordered list, drawn left to right in the right wing. That is
 * the whole of the widget system and it is deliberately not more: a plugin ABI
 * would be a support burden and an injection surface, and the extension point
 * that already exists is kdos-slit, where a gadget is a PROGRAM THAT PRINTS A
 * LINE and its config line is argv.
 *
 * A name this does not know is REPORTED, not ignored — the same promise
 * comp.conf makes, and for the same reason: a typo that produced silence is
 * indistinguishable from a widget that does nothing.
 *
 * The file is absent by default and the default list is the shipped bar.
 */
enum {
	W_CLOCK = 0, W_BATTERY, W_VOLUME, W_NET, W_RESTART, W_PRIVACY,
	W_TRAY, W_PAGER, W_MPRIS, W_CPU, W_CLIP, W_MEDIA, W_NOTIFY,
	W_STUTTER, W_MORE, W_N
};

static const char *const WIDGET_NAMES[W_N] = {
	"clock", "battery", "volume", "net", "restart", "privacy",
	"tray", "pager", "mpris", "cpu", "clipboard", "media", "notify",
	"stutter", "more"
};

/* Left to right as they appear on the bar. The clock is last because the
 * layout runs right to left and the clock is the thing that must not move. */
/*
 * `more` sits immediately RIGHT of the tray, which is where every desktop that
 * has one puts it — and here it is also arithmetic. The chevron is a
 * TRAY-SHAPED cell: two columns, no gap of its own, so it abuts the tray items
 * the way they abut each other. That is what makes it free: hiding fcitx5's
 * item gave two columns back and the chevron takes exactly those two. An
 * ordinary applet tile would have cost four, and four is the NET chart on an
 * eighty-column bar with one window open — measured, and the reason this is not
 * `applet_tile`.
 */
static const int WIDGETS_SHIPPED[W_N] = { W_PAGER, W_TRAY, W_MORE, W_MEDIA,
					  W_PRIVACY, W_MPRIS, W_CLIP, W_CPU,
					  W_STUTTER, W_RESTART, W_NET, W_VOLUME,
					  W_BATTERY, W_NOTIFY, W_CLOCK };
static int widgets[W_N] = { W_PAGER, W_TRAY, W_MORE, W_MEDIA, W_PRIVACY,
			    W_MPRIS, W_CLIP, W_CPU, W_STUTTER, W_RESTART,
			    W_NET, W_VOLUME, W_BATTERY, W_NOTIFY, W_CLOCK };
static int nwidgets = W_N;

/*
 * ── THE OVERFLOW: A CHEVRON THAT DOES NOT MOVE ────────────────────────────
 *
 * HALF THE RIGHT WING IS OCCASIONAL. The stutter chip, the restart mark, the
 * clipboard depth and the removable-media count each appear when they have
 * something to say and vanish when they do not — and every appearance is four
 * columns, so the wing changes width, the meters strip beside it slides, and
 * the whole bar reads as though it jerks. Reported exactly that way: "the
 * orange icon always comes with many warnings, and when it disappears it
 * shrinks the bar".
 *
 * The fix is the one Windows has shipped since XP and every phone since: the
 * occasional items live behind ONE cell of fixed width, which is present
 * whether or not anything is in it. The wing stops moving because nothing in
 * it comes and goes, and the chevron says how many things are hiding — a
 * NUMBER, so a glance still answers "is anything wrong" without the popup.
 *
 * `overflow = stutter restart clipboard` in panel.conf is the list, and it is
 * a list of the SAME widget names the `right =` line uses: a widget is either
 * on the bar or behind the chevron, and moving one is moving a word from one
 * line to the other. `tray_hide = <ids>` does the same for tray items, which
 * is the only answer this desktop has for an item whose menu it cannot draw
 * (see the fcitx5 note in draw_tray).
 */
#define OV_MAX 24

struct ovitem {
	char key[24];		/* what kdos-status runs — see write_overflow */
	char icon[48];
	char label[48];
	char detail[96];
	int warn;
	char service[SH_TRAY_NAME];	/* tray items only */
	char path[SH_TRAY_NAME];
};

static struct ovitem ov[OV_MAX];
static int nov;
static int ov_warn;		/* anything in there wants attention */

/* Which widgets are behind the chevron rather than on the bar. */
static int in_overflow[W_N] = { 0 };
/* SNI ids that are not drawn on the bar, lowercased. */
static char tray_hide[8][64];
static int ntray_hide;

/* One name, lowercased, into the tray_hide table. */
static void tray_hide_add(const char *id)
{
	if (ntray_hide >= (int)(sizeof(tray_hide) / sizeof(tray_hide[0])))
		return;
	size_t n = 0;
	for (; id[n] && n < sizeof(tray_hide[0]) - 1; n++)
		tray_hide[ntray_hide][n] =
			(id[n] >= 'A' && id[n] <= 'Z') ? (char)(id[n] - 'A' + 'a')
						       : id[n];
	tray_hide[ntray_hide][n] = '\0';
	if (n)
		ntray_hide++;
}

static int tray_hidden(const char *id)
{
	if (!id || !*id)
		return 0;
	for (int i = 0; i < ntray_hide; i++)
		if (!strcasecmp(id, tray_hide[i]))
			return 1;
	return 0;
}

static void load_widgets(void)
{
	char path[512], line[512];
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	FILE *f;

	/*
	 * EVERY DEFAULT IS RESTORED FIRST, because this runs again on SIGHUP:
	 * kdos-settings writes panel.conf and signals the panel, and a reload
	 * that only ever ADDED would leave a widget hidden after the line that
	 * hid it was deleted. The file is the whole state, every time.
	 *
	 * They are set BEFORE the file is opened because there usually is no
	 * file at all: the three diagnostics go behind the chevron, and
	 * fcitx5's tray item goes with them.
	 *
	 * FCITX5 IS THE ONE ITEM EVERY KDOS SESSION HAS AND IT CANNOT BE
	 * CLICKED. It publishes `ItemIsMenu`, so its Activate means "show my
	 * menu" and the menu is com.canonical.dbusmenu, which this tray does
	 * not render (a stated gap, not an oversight — it is a second protocol
	 * with a nested-variant layout tree). So the keyboard picture beside
	 * the clock answered nothing at all, which is what "the keyboard icon
	 * does not do anything, why is it needed there" is. It is not needed
	 * there: it is behind the chevron, where the popup can at least SAY
	 * what it is, and `tray_hide =` in panel.conf brings it back.
	 */
	memcpy(widgets, WIDGETS_SHIPPED, sizeof(widgets));
	nwidgets = W_N;
	meters_sel[0] = MT_CPU;
	meters_sel[1] = MT_RAM;
	meters_sel[2] = MT_NET;
	nmeters_sel = 3;
	task_labels = TL_AUTO;
	for (int i = 0; i < W_N; i++)
		in_overflow[i] = 0;
	ntray_hide = 0;
	in_overflow[W_STUTTER] = in_overflow[W_RESTART] = in_overflow[W_CLIP] = 1;
	tray_hide_add("fcitx");
	tray_hide_add("fcitx5");
	tray_hide_add("org.fcitx.fcitx5");

	if (cfg && *cfg)
		snprintf(path, sizeof(path), "%s/kdos/panel.conf", cfg);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.config/kdos/panel.conf", home);
	else
		return;
	f = fopen(path, "r");
	if (!f)
		return;			/* absent is the shipped bar */

	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '#')
			continue;
		char *eq = strchr(p, '=');
		if (!eq)
			continue;

		/*
		 * `right = pager tray … clock` — the notification area, left
		 * to right. A name this does not know is REPORTED, not
		 * ignored: a typo that produced silence is indistinguishable
		 * from a widget that does nothing.
		 */
		if (!strncmp(p, "right", 5)) {
			nwidgets = 0;
			for (char *tok = strtok(eq + 1, " \t\r\n");
			     tok && nwidgets < W_N;
			     tok = strtok(NULL, " \t\r\n")) {
				int found = -1;
				for (int i = 0; i < W_N; i++)
					if (!strcmp(WIDGET_NAMES[i], tok))
						found = i;
				if (found < 0) {
					fprintf(stderr,
						"kdos-shell: panel.conf: no "
						"widget named `%s` — "
						"ignored\n", tok);
					continue;
				}
				widgets[nwidgets++] = found;
			}
			continue;
		}

		/*
		 * `overflow = stutter restart clipboard` — which of the
		 * notification area's widgets live behind the chevron instead
		 * of on the bar. The names are the `right =` names, because it
		 * is the same set of widgets in a different place. An empty
		 * value is an honest "nothing hidden", and the chevron then
		 * draws only when a tray item is hidden.
		 */
		if (!strncmp(p, "overflow", 8)) {
			for (int i = 0; i < W_N; i++)
				in_overflow[i] = 0;
			for (char *tok = strtok(eq + 1, " \t\r\n"); tok;
			     tok = strtok(NULL, " \t\r\n")) {
				int found = -1;
				for (int i = 0; i < W_N; i++)
					if (!strcmp(WIDGET_NAMES[i], tok))
						found = i;
				if (found < 0 || found == W_MORE) {
					fprintf(stderr,
						"kdos-shell: panel.conf: `%s` "
						"cannot go in the overflow — "
						"ignored\n", tok);
					continue;
				}
				in_overflow[found] = 1;
			}
			continue;
		}

		/*
		 * `tray_hide = fcitx5 …` — StatusNotifierItem ids that are not
		 * drawn on the bar. They are listed in the overflow popup
		 * instead, which is the only thing this desktop can honestly
		 * offer an item whose menu it cannot draw.
		 */
		if (!strncmp(p, "tray_hide", 9)) {
			ntray_hide = 0;
			for (char *tok = strtok(eq + 1, " \t\r\n"); tok;
			     tok = strtok(NULL, " \t\r\n"))
				tray_hide_add(tok);
			continue;
		}

		/*
		 * `meters = cpu ram net` — which charts, and in what order.
		 * The order is the order of IMPORTANCE: a narrow bar drops
		 * them from the right, so whatever is written last is what
		 * goes first. `meters =` with nothing after it is an honest
		 * off.
		 */
		if (!strncmp(p, "meters", 6)) {
			nmeters_sel = 0;
			for (char *tok = strtok(eq + 1, " \t\r\n");
			     tok && nmeters_sel < MT_N;
			     tok = strtok(NULL, " \t\r\n")) {
				int found = meter_by_key(tok);
				if (found < 0) {
					fprintf(stderr,
						"kdos-shell: panel.conf: no "
						"meter named `%s` — "
						"ignored\n", tok);
					continue;
				}
				meters_sel[nmeters_sel++] = found;
			}
			continue;
		}

		/*
		 * `task_labels = auto | yes | no` — whether a window button
		 * carries its name. `auto` is the bar deciding for itself and
		 * is what it has always done; the other two are somebody who
		 * knows what they want.
		 */
		if (!strncmp(p, "task_labels", 11)) {
			char v[32] = "";
			sscanf(eq + 1, "%31s", v);
			if (!strcmp(v, "yes") || !strcmp(v, "on"))
				task_labels = TL_ALWAYS;
			else if (!strcmp(v, "no") || !strcmp(v, "off"))
				task_labels = TL_NEVER;
			else if (!strcmp(v, "auto"))
				task_labels = TL_AUTO;
			else
				fprintf(stderr,
					"kdos-shell: panel.conf: task_labels "
					"takes auto|yes|no, not `%s`\n", v);
			continue;
		}
	}
	fclose(f);
}

/*
 * How many things are on the clipboard, or -1 when nothing is keeping them.
 *
 * ONE local socket round trip every two seconds. A unix socket in the session's
 * own runtime directory answers in microseconds — there is no name to resolve
 * and no network — and the timeout is set so that a wedged daemon costs a tenth
 * of a second once rather than a frame every time. The alternative, a
 * subscription, would be a second protocol for a number that fits in a cell.
 */
/*
 * THE NOTIFICATION CENTRE'S BADGE — unseen, kept, and whether Do Not Disturb
 * is on, in ONE line off kdos-notifyd's socket.
 *
 * Once a second, one short connection, exactly like the clipboard depth below
 * it: the daemon already owns the list and the panel needs three integers out
 * of it, so nothing here parses a notification and nothing here keeps state
 * that could disagree with the daemon's.
 */
static int notify_unseen = -1, notify_kept, notify_dnd;

static void notify_poll(void)
{
	static time_t asked;
	time_t now = time(NULL);
	const char *run = getenv("XDG_RUNTIME_DIR");
	char path[256], buf[64] = { 0 };
	int fd;

	if (asked == now)
		return;
	asked = now;
	notify_unseen = -1;
	if (!run || !*run)
		return;
	snprintf(path, sizeof(path), "%s/kdos-notify.sock", run);
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		(void)!write(fd, "count\n", 6);
		if (read(fd, buf, sizeof(buf) - 1) > 0) {
			int a = 0, b = 0, d = 0;

			if (sscanf(buf, "%d %d %d", &a, &b, &d) >= 3) {
				notify_unseen = a;
				notify_kept = b;
				notify_dnd = d;
			}
		}
	}
	close(fd);
}

/*
 * ── THE STUTTER CHIP, and nothing else on any desktop has one ──────────────
 *
 * kdos-comp reports every late frame on `$XDG_RUNTIME_DIR/kdos-frames.sock` —
 * one NDJSON object per miss, carrying how many frames were dropped and what
 * the compositor's own render cost was. `kdos stutter` joins that to PSI and
 * /proc and names who was busy; it is the one thing on this machine that can
 * answer "why did that jerk", and until now it was reachable only by knowing
 * the subcommand's name and typing it AFTER the moment had passed.
 *
 * So the panel holds the socket open and counts. The cell appears when the
 * desktop has actually missed frames in the last ten seconds and goes away
 * when it stops, which is the honest shape: an indicator that is always up
 * says nothing, and one that lights while the screen is stuttering is pointing
 * at the thing you are already looking at.
 *
 * THE READ MUST NEVER BLOCK THE FRAME LOOP — the same rule the compositor
 * keeps on its end. Non-blocking connect and read, a reconnect no more often
 * than every ten seconds, and a consumer that falls behind loses lines, which
 * the protocol is designed for.
 *
 * There is no JSON parser in this binary and this does not add one: the field
 * is written by kdos-frames.c three lines from here in the same repository,
 * and `"dropped":` is scanned for literally.
 */
#define FR_WINDOW_MS 10000
#define FR_MIN 3			/* below this it is not worth a cell */
#define FR_RING 32

static int frames_fd = -1;
static time_t frames_try;
static struct {
	int64_t at;
	int n;
} fr_ring[FR_RING];
static int fr_head;
static int panel_stutter;		/* drops in the window, or 0 */

static void frames_poll(void)
{
	char buf[4096];
	ssize_t r;

	if (frames_fd < 0) {
		const char *run = getenv("XDG_RUNTIME_DIR");
		time_t now = time(NULL);
		char path[256];

		if (!run || !*run || now - frames_try < 10)
			goto tally;
		frames_try = now;
		snprintf(path, sizeof(path), "%s/kdos-frames.sock", run);
		frames_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC |
						   SOCK_NONBLOCK,
				   0);
		if (frames_fd < 0)
			goto tally;
		struct sockaddr_un addr = { .sun_family = AF_UNIX };
		snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
		if (connect(frames_fd, (struct sockaddr *)&addr,
			    sizeof(addr)) != 0 &&
		    errno != EINPROGRESS) {
			close(frames_fd);
			frames_fd = -1;
		}
		goto tally;
	}

	while ((r = read(frames_fd, buf, sizeof(buf) - 1)) > 0) {
		buf[r] = '\0';
		for (char *p = buf; (p = strstr(p, "\"dropped\":")); ) {
			int n = atoi(p + 10);

			p += 10;
			if (n < 1)
				continue;
			fr_ring[fr_head].at = panel_now_ms();
			fr_ring[fr_head].n = n;
			fr_head = (fr_head + 1) % FR_RING;
		}
	}
	/* EOF is the compositor going away — which, for a panel, is the
	 * session ending anyway. Zero is the only value that means it. */
	if (r == 0) {
		close(frames_fd);
		frames_fd = -1;
	}

tally: {
	int64_t now = panel_now_ms();
	int total = 0;

	for (int i = 0; i < FR_RING; i++)
		if (fr_ring[i].at && now - fr_ring[i].at <= FR_WINDOW_MS)
			total += fr_ring[i].n;
	panel_stutter = total >= FR_MIN ? total : 0;
	}
}

static int clip_depth(void)
{
	static time_t asked;
	static int cached = -1;
	time_t now = time(NULL);
	const char *run = getenv("XDG_RUNTIME_DIR");
	char path[256], buf[32] = {0};
	int fd;

	if (asked == now)
		return cached;
	asked = now;
	cached = -1;
	if (!run || !*run)
		return -1;
	snprintf(path, sizeof(path), "%s/kdos-clip.sock", run);
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
	struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		(void)!write(fd, "count\n", 6);
		if (read(fd, buf, sizeof(buf) - 1) > 0)
			cached = atoi(buf);
	}
	close(fd);
	return cached;
}

/*
 * How many removable filesystems are attached, and how many of them are
 * mounted — read from /sys and /proc, NOT from kdos-mountd.
 *
 * The daemon is the thing that MOUNTS; asking it once per tick would be a
 * socket round trip on the frame path for a number the kernel already exports
 * as text, which is the same argument the battery and the network readouts are
 * built on. The two answers cannot disagree, because they are reading the same
 * two files the daemon reads.
 */
/*
 * IS THIS DISK THE SYSTEM'S OWN? — mounted anywhere except /media.
 *
 * The count used to be "removable, full stop", which on the live ISO is the
 * disc this machine BOOTED FROM: the panel read `2` with nothing plugged in,
 * and the manager the applet opens — kdos-mountd, which refuses the boot
 * medium and anything in fstab — offered nothing at all. A readout that
 * disagrees with the window it opens is worse than no readout.
 *
 * The same two files the daemon reads, and the same conclusion from them: a
 * device the system has already mounted somewhere of its own is not a stick
 * somebody just plugged in.
 */
static int media_is_system(const char *disk)
{
	FILE *f = fopen("/proc/mounts", "r");
	char line[512];
	size_t dl = strlen(disk);
	int sys = 0;

	if (!f)
		return 0;
	while (!sys && fgets(line, sizeof(line), f)) {
		char dev[256], mnt[256];

		if (sscanf(line, "%255s %255s", dev, mnt) != 2)
			continue;
		if (strncmp(dev, "/dev/", 5) || strncmp(dev + 5, disk, dl))
			continue;
		/* /dev/sda and /dev/sda1 both belong to sda; /dev/sdab does
		 * not, so what follows the name has to be a partition number
		 * or nothing at all. */
		for (const char *p = dev + 5 + dl; *p; p++)
			if (*p < '0' || *p > '9')
				goto next;
		if (strncmp(mnt, "/media/", 7))
			sys = 1;
next:
		continue;
	}
	fclose(f);
	return sys;
}

static int media_count(int *mounted)
{
	DIR *d = opendir("/sys/block");
	struct dirent *e;
	int n = 0;

	*mounted = 0;
	if (!d)
		return 0;
	/*
	 * THE LIVE ISO CANNOT BE FOUND IN /proc/mounts AT ALL, which is why the
	 * mounted-somewhere test above is not enough on its own. The initramfs
	 * mounts the disc and then `switch_root` MS_MOVEs the new root, so
	 * those mounts stay in the OLD namespace and the running system has one
	 * line for the whole arrangement: `overlay / overlay …`. kdos-mountd
	 * answers that by refusing every ISO9660 medium in a live session, and
	 * this has to reach the same conclusion by the same evidence or the
	 * panel counts a stick the manager will not offer — which is exactly
	 * what it did: `2`, on a machine with nothing plugged in.
	 */
	int live = 0;
	FILE *mf = fopen("/proc/mounts", "r");

	if (mf) {
		char line[512];

		while (fgets(line, sizeof(line), mf)) {
			char dev[256], mnt[256], type[64];

			if (sscanf(line, "%255s %255s %63s", dev, mnt, type) ==
				    3 &&
			    !strcmp(mnt, "/") && !strcmp(type, "overlay"))
				live = 1;
		}
		fclose(mf);
	}

	while ((e = readdir(d))) {
		char path[512], buf[64];

		if (e->d_name[0] == '.' || !strncmp(e->d_name, "loop", 4) ||
		    !strncmp(e->d_name, "ram", 3) ||
		    !strncmp(e->d_name, "dm-", 3) ||
		    !strncmp(e->d_name, "zram", 4))
			continue;
		/*
		 * A FLOPPY THAT IS NOT THERE. Every i440fx machine QEMU builds
		 * has an `fd0` with `removable = 1` and a size of eight
		 * sectors, and no machine anybody is running this on has a
		 * floppy drive at all. It was half of the `2`.
		 */
		if (!strncmp(e->d_name, "fd", 2))
			continue;
		/* The optical drive in a live session is the operating
		 * system — see `live` above. */
		if (live && !strncmp(e->d_name, "sr", 2))
			continue;
		snprintf(path, sizeof(path), "/sys/block/%s/removable",
			 e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) != 0 ||
		    atoi(buf) != 1)
			continue;
		/* An empty drive is a drive, not a medium: a CD reader with no
		 * disc in it reports removable=1 and a size of zero. */
		snprintf(path, sizeof(path), "/sys/block/%s/size", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0 && atoll(buf) <= 0)
			continue;
		if (media_is_system(e->d_name))
			continue;
		n++;
	}
	closedir(d);

	/* Mounted means "mounted where the daemon puts them": /media. Anything
	 * else is somebody's own fstab entry and not this widget's business. */
	FILE *f = fopen("/proc/mounts", "r");
	if (f) {
		char line[512];
		while (fgets(line, sizeof(line), f)) {
			char dev[256], mnt[256];
			if (sscanf(line, "%255s %255s", dev, mnt) == 2 &&
			    !strncmp(mnt, "/media/", 7))
				(*mounted)++;
		}
		fclose(f);
	}
	return n;
}

/* ── the meters: CPU, memory and the network ───────────────────────────────
 *
 * THE SAMPLE INTERVAL IS A CLOCK, NOT THE DRAW LOOP, and getting that wrong is
 * the whole reason the CPU readout was unreadable.
 *
 * A rate is a difference over an elapsed time, and the panel's loop is woken
 * by EVENTS: the poll timeout is a second, but a pointer crossing the bar, a
 * window appearing, a tray property arriving or a frame callback all return
 * from it early. The old reading re-sampled /proc/stat on every one of those
 * and divided by whatever interval had happened to elapse — a few jiffies over
 * a few milliseconds — so moving the mouse made `CPU 0%` and `CPU 100%` flash
 * at pointer speed. It was not a rendering fault and no amount of redraw
 * throttling would have fixed it: the NUMBER was wrong.
 *
 * So the meters keep their own monotonic deadline and every reader gets the
 * last completed sample.
 *
 * AND THE DEADLINE HAS TO BE THE POLL'S TOO, which is the other half of the
 * same bug and is what "the graph does not flow, it sticks and then jumps"
 * was. The loop waited a flat second and the sampler wanted a sample every
 * half one: a pointer crossing the bar at 490 ms returned from the poll early,
 * the tick found the deadline not yet due, and the NEXT wait ran its full
 * length — so the sample landed at 990 ms and the one after it at 1490. The
 * chart advances one pixel per sample, so an irregular sample interval is an
 * irregular chart, and moving the mouse was enough to cause it. The panel's
 * wait is now shortened to whatever is left of the interval, exactly as it
 * already is for the launch pulse and as libkwl's own poll is for a key
 * repeat.
 *
 * HALF A SECOND, not a whole one. The band is around sixty pixels wide and one
 * sample is one pixel: at a second apart a chart takes a minute to fill and
 * creeps a pixel at a time, which reads as a picture that has stopped. Twice
 * that is a plot that visibly moves and a window (KPR_HIST samples, so a
 * little over a minute) that still says something. It is not faster because it
 * is more accurate — a half-second CPU sample is noisier, which is what the
 * smoothing below is for — it is faster because a chart is a thing in motion.
 *
 * The displayed value is SMOOTHED and the history is NOT. A one-second CPU
 * sample genuinely jumps between 4% and 60% on an idle desktop, and a number
 * that changes completely every second is one people stop reading — so the
 * label is an exponential average. The sparkline plots the raw samples,
 * because the point of a chart is the spikes.
 */
/*
 * The ring is KPR_HIST deep — a hundred and twenty-eight samples, which at two
 * a second is a little over a minute, and the number is set by the CHART
 * rather than by taste. The tile draws one sample per PIXEL, and a band is
 * around sixty pixels wide at the shipped cell size and twice that at scale 2;
 * thirty-two samples filled half of one and left the rest blank, which reads
 * as a chart that has stopped rather than as a machine that has been quiet.
 */
#define MET_MS 500		/* the sample interval, in milliseconds */
/*
 * A GRIDLINE EVERY TEN SECONDS, AND IT IS WHAT MAKES AN IDLE CHART A CHART.
 *
 * Flat data draws the same picture whatever it does next, so a quiet link
 * rendered a still image: reported as "when the net activity stops the graphs
 * don't move". Nothing was frozen — there was simply nothing on the band whose
 * position depended on TIME. Every monitor that reads well at rest (netdata,
 * btop, Activity Monitor) puts a scale under the trace for exactly this, and on
 * a scrolling plot the scale is what shows the scroll.
 *
 * The ticks are keyed to the ABSOLUTE sample number, so they march left with
 * the samples they were drawn beside rather than sitting at fixed columns.
 */
#define MET_GRID 20		/* samples between gridlines — ten seconds */

/*
 * Every sample ever taken, which is what a gridline's phase is measured in and
 * what tells the tile's hash that time has passed. Without it a band of zeroes
 * hashes the same twice and the tile is never re-rastered, so the grid would
 * be as still as the data it exists to move.
 */
static uint64_t met_seq;

/*
 * The RING is libkproc's; the exponential average is not. They are two
 * different smoothings answering two different questions — the ring keeps raw
 * samples and feeds the plot, and `shown` is the label, which would otherwise
 * change completely twice a second and stop being read.
 */
struct meter {
	KprHist h;
	double shown;		/* the smoothed value the label prints */
	int have;		/* a first sample has landed */
};

/*
 * RX AND TX ARE TWO MEASUREMENTS, and summing them was hiding the one thing
 * anybody watches a network meter for. "269 kB/s" does not say whether this
 * machine is downloading or being uploaded from, and those are different
 * questions with different answers. They are drawn mirrored about a midline —
 * received above, sent below — which is the shape every network monitor has
 * used since MRTG and is readable at sixteen pixels a side.
 */
/*
 * SEVEN SERIES, FIVE METERS. The mirrored pair is one meter drawn as two
 * series about a midline, because a rate has a direction and the sum of the
 * two answers neither question anybody opens a network meter to ask.
 */
static struct meter met_cpu, met_ram, met_disk;
static struct meter met_rx, met_tx;		/* network, received / sent  */
static struct meter met_rd, met_wr;		/* disk, read / written      */

static int64_t panel_now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void meter_push(struct meter *m, double v, double smooth)
{
	kpr_hist_push(&m->h, v);
	m->shown = m->have ? m->shown + (v - m->shown) * smooth : v;
	m->have = 1;
}

/*
 * A SERIES THAT SKIPS A TICK IS A SERIES ON ITS OWN CLOCK.
 *
 * Every band here plots one sample per PIXEL and the gridlines are keyed to
 * `met_seq`, so a reading that is momentarily unavailable — /proc/stat's
 * aggregate not having advanced, an interface that came and went, a disk whose
 * stats could not be read — must not simply go unrecorded. The series would
 * then be SHORTER than the one beside it: its columns would cover a different
 * span of time, its gridlines would sit under a different second, and the two
 * charts would creep out of step for the rest of the session. The last value
 * is carried forward instead, which is exactly what the filesystem meter
 * already does deliberately between its ten-second reads.
 *
 * A series with no sample AT ALL is left empty rather than held at zero: an
 * absent reading is not a reading of nothing, and `have` is what the applets
 * test to decide whether they may print a number.
 */
static void meter_hold(struct meter *m)
{
	if (m->have)
		meter_push(m, kpr_hist_at(&m->h, m->h.n - 1), 1.0);
}

/* The most recent `cols` samples, oldest first — what a chart wants. */
static const double *meter_series(const struct meter *m, int *n)
{
	*n = m->h.n;
	return m->h.v;
}

/* /proc/stat's aggregate line. Returns 0 and fills the pair, or -1. */
static int read_cpu_jiffies(unsigned long long *total, unsigned long long *idle)
{
	unsigned long long v[8] = { 0 };
	char buf[256];
	FILE *f = fopen("/proc/stat", "r");

	if (!f)
		return -1;
	if (!fgets(buf, sizeof(buf), f) ||
	    sscanf(buf, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0],
		   &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]) < 4) {
		fclose(f);
		return -1;
	}
	fclose(f);
	*total = 0;
	for (int i = 0; i < 8; i++)
		*total += v[i];
	*idle = v[3] + v[4];		/* idle + iowait */
	return 0;
}

/*
 * Memory pressure as a percentage, from MemAvailable.
 *
 * NOT `MemTotal - MemFree`: Linux spends every spare page on cache and that
 * arithmetic reports a healthy machine at 95% used, which is the number every
 * naive memory monitor has shown since 2005 and the reason people learned to
 * ignore them. MemAvailable is the kernel's own estimate of what a new
 * allocation could actually get, reclaim included.
 */
static int read_mem_used(double *pct)
{
	char line[256];
	double total = 0, avail = 0;
	FILE *f = fopen("/proc/meminfo", "r");

	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f)) {
		double v;
		if (sscanf(line, "MemTotal: %lf kB", &v) == 1)
			total = v;
		else if (sscanf(line, "MemAvailable: %lf kB", &v) == 1)
			avail = v;
	}
	fclose(f);
	if (total <= 0)
		return -1;
	if (avail > total)
		avail = total;
	*pct = (total - avail) * 100.0 / total;
	return 0;
}

/*
 * How full the root filesystem is.
 *
 * `/` and not every mount: a panel meter has one number and the disk a person
 * means is the one their session is on. It is also the one that fills up. A
 * live ISO's root is an overlay whose free space is the tmpfs upper layer,
 * which is exactly what somebody running from the stick wants to watch.
 */
static int read_disk_used(double *pct)
{
	struct statvfs vfs;

	if (statvfs("/", &vfs) != 0 || vfs.f_blocks == 0)
		return -1;
	/* f_bavail, not f_bfree: the reserved blocks are not available to the
	 * user and counting them as free reports a full disk as having room. */
	double total = (double)vfs.f_blocks;
	double avail = (double)vfs.f_bavail;
	if (avail > total)
		avail = total;
	*pct = (total - avail) * 100.0 / total;
	return 0;
}

/*
 * Bytes over every real interface, summed, as two directions.
 *
 * Loopback is excluded — it is this machine talking to itself, and a desktop
 * that showed 200 MB/s because a compile was piping through a socket would be
 * reporting the wrong thing.
 */
static int read_net_bytes(unsigned long long *rx, unsigned long long *tx)
{
	DIR *d = opendir("/sys/class/net");
	struct dirent *e;
	unsigned long long rsum = 0, tsum = 0;
	int any = 0;

	if (!d)
		return -1;
	while ((e = readdir(d))) {
		char path[512], buf[64];
		if (e->d_name[0] == '.' || !strcmp(e->d_name, "lo"))
			continue;
		snprintf(path, sizeof(path),
			 "/sys/class/net/%s/statistics/rx_bytes", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0) {
			rsum += strtoull(buf, NULL, 10);
			any = 1;
		}
		snprintf(path, sizeof(path),
			 "/sys/class/net/%s/statistics/tx_bytes", e->d_name);
		if (sh_read_line(path, buf, sizeof(buf)) == 0) {
			tsum += strtoull(buf, NULL, 10);
			any = 1;
		}
	}
	closedir(d);
	if (!any)
		return -1;
	*rx = rsum;
	*tx = tsum;
	return 0;
}

/*
 * Sectors read and written, from /proc/diskstats.
 *
 * WHOLE DISKS ONLY, and that is the whole of getting this right: the file
 * lists `sda` and `sda1` and `sda2`, and summing the lot counts every byte
 * two or three times. A partition is recognised by having a parent — sysfs
 * says so directly, since `/sys/class/block/<name>/partition` exists only for
 * one. `loop`, `ram` and `zram` are dropped as well: a live ISO's squashfs
 * loop would otherwise report the whole of every file the session reads.
 *
 * A sector here is 512 bytes BY DEFINITION of this interface, whatever the
 * device's own sector size — the kernel documents diskstats in 512-byte units
 * and reading `queue/hw_sector_size` instead is the classic way to get this
 * eight times wrong on a 4K drive.
 */
static int read_disk_io(unsigned long long *rd, unsigned long long *wr)
{
	FILE *f = fopen("/proc/diskstats", "r");
	char line[512];
	unsigned long long rs = 0, ws = 0;
	int any = 0;

	if (!f)
		return -1;
	while (fgets(line, sizeof(line), f)) {
		unsigned mj, mn;
		char name[64];
		unsigned long long rio, rmg, rsec, rms, wio, wmg, wsec;
		if (sscanf(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu",
			   &mj, &mn, name, &rio, &rmg, &rsec, &rms, &wio, &wmg,
			   &wsec) != 10)
			continue;
		if (!strncmp(name, "loop", 4) || !strncmp(name, "ram", 3) ||
		    !strncmp(name, "zram", 4))
			continue;
		char path[256];
		snprintf(path, sizeof(path), "/sys/class/block/%s/partition",
			 name);
		if (access(path, F_OK) == 0)
			continue;		/* a partition of a disk above */
		rs += rsec;
		ws += wsec;
		any = 1;
	}
	fclose(f);
	if (!any)
		return -1;
	*rd = rs * 512;
	*wr = ws * 512;
	return 0;
}

/*
 * One sampling tick, at most once per MET_MS however often this is called.
 *
 * The FIRST call takes a baseline and publishes nothing: a rate computed
 * against zero reads 100% for the first second of every session, which is a
 * gauge people learn to distrust.
 */
/*
 * When the next sample is due, in milliseconds. The panel's poll takes this as
 * its ceiling — see the header of this block: a wait that outruns the sample
 * interval is what made the chart jerk.
 */
static int64_t meters_last_ms;

static int meters_due_in(void)
{
	int64_t rem;

	if (!meters_last_ms)
		return 0;
	rem = MET_MS - (panel_now_ms() - meters_last_ms);
	return rem < 0 ? 0 : (int)rem;
}

static void meters_sample(void)
{
	static unsigned long long last_total, last_idle;
	static unsigned long long last_rx, last_tx, last_rd, last_wr;
	static int primed, disk_countdown;
	int64_t now = panel_now_ms();
	int64_t dt_ms;

	if (meters_last_ms && now - meters_last_ms < MET_MS)
		return;
	dt_ms = meters_last_ms ? now - meters_last_ms : 0;
	meters_last_ms = now;
	met_seq++;

	unsigned long long total = 0, idle = 0, rx = 0, tx = 0, rd = 0, wr = 0;
	int have_cpu = read_cpu_jiffies(&total, &idle) == 0;
	int have_net = read_net_bytes(&rx, &tx) == 0;
	int have_dio = read_disk_io(&rd, &wr) == 0;

	if (!primed) {
		primed = 1;
		last_total = total;
		last_idle = idle;
		last_rx = rx;
		last_tx = tx;
		last_rd = rd;
		last_wr = wr;
		return;
	}

	if (have_cpu && total > last_total) {
		unsigned long long dt = total - last_total;
		unsigned long long di = idle > last_idle ? idle - last_idle : 0;
		double busy = 100.0 - (double)di * 100.0 / (double)dt;
		if (busy < 0)
			busy = 0;
		if (busy > 100)
			busy = 100;
		/* 0.2 at two samples a second is the same time constant 0.35
		 * had at one — fast enough that a build shows up at once, slow
		 * enough that the digits are readable while it runs. The CHART
		 * plots the raw samples; only the number is averaged. */
		meter_push(&met_cpu, busy, 0.2);
	} else {
		meter_hold(&met_cpu);
	}
	if (have_cpu) {
		last_total = total;
		last_idle = idle;
	}

	double mem;
	if (read_mem_used(&mem) == 0)
		meter_push(&met_ram, mem, 0.35);
	else
		meter_hold(&met_ram);

	/*
	 * The filesystem every ten seconds, not every sample. It moves in
	 * megabytes over minutes, a statvfs on a network mount can block, and
	 * a chart of a number that does not change is a flat line either way.
	 */
	if (--disk_countdown <= 0) {
		double du;
		disk_countdown = 10000 / MET_MS;
		if (read_disk_used(&du) == 0)
			meter_push(&met_disk, du, 1.0);
		else
			meter_hold(&met_disk);
	} else {
		meter_hold(&met_disk);
	}

	if (dt_ms > 0 && have_net) {
		/* A counter that went BACKWARDS is a device that came and went,
		 * not a negative rate. */
		double r = rx >= last_rx ? (double)(rx - last_rx) * 1000.0 /
						   (double)dt_ms
					 : 0;
		double t = tx >= last_tx ? (double)(tx - last_tx) * 1000.0 /
						   (double)dt_ms
					 : 0;
		meter_push(&met_rx, r, 0.35);
		meter_push(&met_tx, t, 0.35);
	} else {
		/* Both halves of a mirrored pair advance together or neither
		 * does: one held while the other moved would put received and
		 * sent a sample out of step for the rest of the session. */
		meter_hold(&met_rx);
		meter_hold(&met_tx);
	}
	if (dt_ms > 0 && have_dio) {
		double r = rd >= last_rd ? (double)(rd - last_rd) * 1000.0 /
						   (double)dt_ms
					 : 0;
		double t = wr >= last_wr ? (double)(wr - last_wr) * 1000.0 /
						   (double)dt_ms
					 : 0;
		meter_push(&met_rd, r, 0.35);
		meter_push(&met_wr, t, 0.35);
	} else {
		meter_hold(&met_rd);
		meter_hold(&met_wr);
	}
	if (have_net) {
		last_rx = rx;
		last_tx = tx;
	}
	if (have_dio) {
		last_rd = rd;
		last_wr = wr;
	}

}

/* The percentage the CPU applet prints, or -1 before the first sample. */
static int cpu_percent(void)
{
	return met_cpu.have ? (int)(met_cpu.shown + 0.5) : -1;
}

/* ── the taskbar ───────────────────────────────────────────────────────────
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │▓▓ start│ ▪ ▪ │[▪ foot ×2  ][▪ firefox-esr ]│ 1 2 3 4 │K ●MIC 41% 21:07│
 *  │▓▓      │     │                             │         │      Sat 16 Aug│
 *  └──────────────────────────────────────────────────────────────────────┘
 *
 * ONE BAR, ON THE BOTTOM EDGE. There used to be two — a GNOME-2 menu bar at the
 * top and a GNOME-2 second panel at the bottom — which is two processes, two
 * layouts, two hit maps and the window list drawn TWICE. It also cost two
 * exclusive zones: on the shipped 1280x800 with a 32-pixel font that is 8% of
 * the screen spent on chrome that says the same thing at both ends of it.
 *
 * The bottom edge specifically, because a taskbar is AIMED AT and the bottom
 * edge is the cheapest target a pointer has — it cannot be overshot. The top
 * edge is equally cheap and is where a menu bar goes; this desktop has no
 * global menu bar and is not getting one.
 *
 * `panel = bottom | top | off` and `panel_cells = 1 | 2` in comp.conf keep the
 * old shape reachable: `panel = top` is this same code with a different anchor.
 * Two rows is the default because that is what makes an icon square — a cell
 * is 16x32, so two cells across two rows is 32x64 and libkicon centres a 32x32
 * picture in it. At one row the bar is labels and glyphs, exactly as before,
 * and nothing on it REQUIRES a picture.
 *
 * The layout is still right to left after the left wing, and that is still
 * load-bearing: the right wing has a fixed width and the window list is what
 * has to give when the screen is narrow. Laying out left to right and hoping
 * is how a clock ends up pushed off a 1280-wide display by one long title.
 * When even that is not enough the Start button collapses to its mark and the
 * quick-launch row goes BEFORE any window button is dropped — a person can
 * reach the menu through the mark, and the window list is what a taskbar IS.
 */

/*
 * The Start button.
 *
 * Filled in the accent and anchored in the corner, which is the entire reason
 * this shape has outlived every desktop that copied it: it is the one control
 * whose position a person never has to look for. It carries the KDOS mark
 * (a picture where there is one, the ≡ glyph where there is not) and the word,
 * and drops the word rather than the mark on a bar too narrow for both.
 */
static int draw_start(struct sh_state *sh, int h, int compact)
{
	const char *word = "start";
	/* The KDOS mark: `kdos-launcher` is what kdos-icons installs into the
	 * hicolor tree as a real PNG, and `start-here` is the name every other
	 * desktop's theme uses for the same thing. Try ours, then theirs, then
	 * the ≡ glyph — which is what a tty and an install with no artwork
	 * draw, and is not a placeholder. */
	/*
	 * TWO CELLS WIDE AND ONE TALL, which on a 16x32 cell is exactly 32x32
	 * — a square, on the same row as the word beside it.
	 *
	 * It used to ask for `2 x h`, and on the two-row bar that is a 32x64
	 * box in which libkicon centres a 32x32 picture: the icon landed
	 * straddling the boundary between the rows while every label sat in
	 * the top one. Nothing was wrong with either half on its own, and the
	 * bar read as though none of it lined up — which is exactly what it
	 * was reported as. Every sprite on this bar is drawn at the content
	 * row now, and the second row is a deliberate detail line (the meters
	 * strip, the clock's date, a window button's title) rather than a
	 * band of empty pixels under the text.
	 */
	int ry = (h - 1) / 2;
	int icon = icons_on ? kicon_slot("kdos-launcher", 2, 1) : -1;
	if (icon < 0 && icons_on)
		icon = kicon_slot("start-here", 2, 1);
	int mark_w = icon >= 0 ? 2 : ktui_utf8_width(menu_mark());
	int lw = compact ? 0 : ktui_utf8_width(word);
	int w = mark_w + (lw ? 1 + lw : 0) + 1;
	/* Lit while the menu is up, not only under the pointer: a Start button
	 * that looks untouched with its own menu open is the one control on
	 * the bar whose state the user cannot see. */
	int lit = sh->hover_start || start_menu_open();

	if (w > ktui_w / 3) {
		lw = 0;
		w = mark_w + 1;
	}

	/*
	 * THE PIXEL BUTTON, where there is more than one row to spend.
	 *
	 * The glyph layout below is correct and looks wrong: on the two-row bar
	 * it is a 32x64 slab of accent with a 32px mark and a 32px word sitting
	 * in its top half, because a cell is a cell and there is no way to say
	 * "this word is forty pixels tall and centred in the button". A tile
	 * says exactly that — see tile.c — and the fallback is the same glyph
	 * layout the bar has always drawn, so a tty, `--dump`, `icons = no`
	 * and a full sprite table are all unaffected.
	 */
	if (h > 1 && !compact && lw) {
		int cell_w = kwl_cell_w(), cell_h = kwl_cell_h();
		int scale = kwl_scale();
		/* The word at ~62% of the button's height, which is the
		 * proportion a label has to a button on every desktop this
		 * shape came from — and the mark square at the same height. */
		/*
		 * SIZED TO WHAT THE BAR CAN AFFORD. At 62% of the button's
		 * height the word came to twelve columns, and on the shipped
		 * 80-column bar a twelve-column Start button plus a
		 * twelve-column quick-launch row forced the whole left wing to
		 * collapse at FOUR windows — measured. Half the height is
		 * still twice the cell font and lands at nine.
		 */
		int px_h = h * cell_h * scale;
		int fsz = px_h * 50 / 100;
		/*
		 * THE MARK IS THE BRAND AND IT WAS THE SMALLEST THING ON THE
		 * BUTTON. At 56% of a 64-pixel bar it is 35 pixels — smaller
		 * than the 40-pixel word beside it, on the one control whose
		 * job is to be recognised from across the room rather than
		 * read. Three quarters of the height puts it at 47, which is
		 * the proportion the mark has to the word on every desktop
		 * this shape came from, and the button grows by one column.
		 */
		int mark_px = px_h * 74 / 100;
		int pad = cell_w * scale / 3;
		int tw = kcell_canvas_text_width(fsz, word);
		int need_px = pad + mark_px + pad + tw + pad;
		int tw_cells = (need_px + cell_w * scale - 1) / (cell_w * scale);

		if (tw_cells >= 3 && tw_cells <= 16 && tw_cells <= ktui_w / 3) {
			/* Everything the picture depends on, and nothing it
			 * does not: the accent is folded in through
			 * kch_tile_reset() on a retint rather than here. */
			uint64_t content = (uint64_t)lit << 40 |
					   (uint64_t)tw_cells << 24 |
					   (uint64_t)fsz << 8 | (uint64_t)h;
			KCellCanvas *cv =
				kch_tile_begin(SH_TILE_START, tw_cells, h,
					      content);
			if (cv) {
				int bg = lit ? KT_WARN : KT_ACCENT;
				int cw_px = kcell_canvas_w(cv);
				int ch_px = kcell_canvas_h(cv);
				kcell_canvas_fill(cv, 0, 0, cw_px, ch_px, bg,
						  255);
				int x = pad;
				pixman_image_t *mk =
					icons_on ? kicon_pixmap("kdos-launcher",
								mark_px,
								mark_px)
						 : NULL;
				if (!mk && icons_on)
					mk = kicon_pixmap("start-here", mark_px,
							  mark_px);
				if (mk) {
					pixman_image_composite32(
						PIXMAN_OP_OVER, mk, NULL,
						kcell_canvas_image(cv), 0, 0,
						0, 0, x,
						(ch_px - mark_px) / 2, mark_px,
						mark_px);
					kicon_pixmap_free(mk);
					x += mark_px + pad;
				}
				/* Centred on the button's own height, which is
				 * the entire point of drawing it as pixels. */
				int asc = kcell_canvas_text_ascent(fsz);
				int hgt = kcell_canvas_text_height(fsz);
				kcell_canvas_text(cv, x,
						  (ch_px - hgt) / 2 + asc, fsz,
						  word, KT_SURFACE);
				kch_tile_commit(SH_TILE_START);
			}
			int slot = kch_tile_slot(SH_TILE_START);
			if (slot >= 0) {
				ktui_draw_sprite(krect(0, 0, tw_cells, h), slot,
						 KT_SURFACE,
						 lit ? KT_WARN : KT_ACCENT);
				sh->start_x = 0;
				sh->start_end = tw_cells;
				return tw_cells;
			}
		}
	}

	ktui_draw_fill(krect(0, 0, w, h), lit ? KT_WARN : KT_ACCENT);
	if (icon >= 0)
		ktui_draw_sprite(krect(0, ry, 2, 1), icon, KT_SURFACE,
				 lit ? KT_WARN : KT_ACCENT);
	else
		ktui_draw_text(0, ry, mark_w, menu_mark(), KT_SURFACE,
			       lit ? KT_WARN : KT_ACCENT, KT_A_NONE);
	if (lw)
		ktui_draw_text(mark_w + 1, ry, lw, word, KT_SURFACE,
			       lit ? KT_WARN : KT_ACCENT, KT_A_NONE);
	sh->start_x = 0;
	sh->start_end = w;
	return w;
}

/*
 * ONE SEPARATOR, EVERYWHERE A SEGMENT ENDS.
 *
 * The bar had two — after the Start button and after the quick-launch row —
 * and nothing between the window list and the status area or in front of the
 * clock, so the right-hand half read as one undifferentiated run of glyphs.
 * A rule costs one column and is what tells somebody where one group of
 * controls stops and the next begins; every panel of this lineage has them in
 * exactly these places.
 *
 * Drawn in KT_DIM against the panel: a full-strength rule would compete with
 * the content it is separating.
 */
/*
 * A SEGMENT BOUNDARY IS THE SAME LINE THE WINDOWS ARE DRAWN WITH.
 *
 * The bar's separators were the single vertical line in the FILL colour,
 * which is 1.63:1 against the panel's own background — below any legibility
 * floor, and the boundaries the bar's layout depends on were therefore
 * invisible in a photograph. They are the DOUBLE vertical, in KT_MID, which
 * is the stroke every KDOS window frame is drawn in and reads at better than
 * four to one.
 */
static void draw_sep(int x, int h)
{
	if (x < 0 || x >= ktui_w)
		return;
	ktui_draw_vline(x, 0, h, KT_G_DVL, KT_MID, KT_SURFACE);
}

/*
 * Quick launch: the pinned entries from ~/.config/kdos/favorites, as pictures
 * when there is artwork and as names when there is not. Two cells each either
 * way, so the row does not reflow when an icon theme is installed.
 */
static int draw_quicklaunch(int x, int limit, int h)
{
	int ry = (h - 1) / 2;
	int drew = 0;

	/*
	 * AN ICON WITH NO LABEL IS CENTRED ON THE BAR'S FULL HEIGHT; an icon
	 * with a label beside it shares that label's row. Both halves of that
	 * rule are visible here: the quick-launch row is pictures and nothing
	 * else, so a 2x2 box (libkicon centres a 32x32 square in 32x64) puts
	 * them on the same optical line as the Start button's own centred
	 * content — while a task button's icon stays on its label's row,
	 * because an icon floating between a name and a title is the
	 * misalignment this bar was reported for.
	 */
	for (int i = 0; i < nfavs; i++) {
		int icon = icons_on ? kicon_slot(favs[i].icon, 2, h) : -1;
		int w = icon >= 0 ? 2 : ktui_utf8_width(favs[i].name);

		if (w > 10)
			w = 10;
		if (x + w + 1 > limit)
			break;	/* spans past here stay empty: not drawn */
		fav_x[i] = x;
		/*
		 * The affordance goes BEHIND the picture, because an icon is
		 * its own shape and tinting it would change what the app looks
		 * like. A hover is the fill colour; a launch alternates
		 * between the fill and the accent at about four hertz, which
		 * reads as a pulse rather than as a flicker.
		 */
		int lit = 0;
		if (fav_anim == i) {
			int64_t age = panel_now_ms() - fav_anim_at;
			if (age < FAV_ANIM_MS)
				lit = ((age / 130) & 1) ? KT_ACCENT : KT_MID;
			else
				fav_anim = -1;
		}
		/* Mid-drag the ROW says where the icon would land: the target
		 * takes the accent and the one being carried goes dim, which
		 * is the whole of the feedback a two-cell picture can give. */
		if (drag_moved && drag_fav >= 0) {
			if (drag_over == i)
				lit = KT_ACCENT;
			else if (drag_fav == i)
				lit = KT_DIM;
		}
		if (!lit && hover_fav == i)
			lit = KT_DIM;
		if (lit)
			ktui_draw_fill(krect(x, 0, 2, h), lit);
		if (icon >= 0) {
			ktui_draw_sprite(krect(x, 0, 2, h), icon, KT_TEXT,
					 lit ? lit : KT_SURFACE);
			x += 2;
		} else {
			x += ktui_draw_text(x, ry, w, favs[i].name, KT_MID,
					    KT_SURFACE, KT_A_NONE);
		}
		fav_end[i] = x;
		x += 1;
		drew = 1;
	}
	if (drew) {
		draw_sep(x, h);
		x += 2;
	}
	return x;
}

/*
 * The pager, CLAMPED: sixteen workspaces on a narrow output pushed it off the
 * left of its own span, the recorded span covered the window list, and a click
 * on an empty stretch of bar switched workspaces. When the squares do not fit,
 * a compact `N/M` readout is drawn instead and the pager records NO span —
 * only what was actually drawn may be hit.
 *
 * It records where it drew each square, not a stride: `<desktops number="12"/>`
 * is a supported thing to write, and from the tenth on a two-cell stride
 * activated the wrong workspace.
 */
/*
 * THE WORKSPACES ARE LITTLE SCREENS, not a row of dots.
 *
 * One dot per workspace on one row of a two-row bar is the widget that was
 * wasting the most space for the least meaning: four cells that say "four" and
 * a shade of green that says "this one", with nothing underneath and no way to
 * tell workspace 2 from workspace 3 without counting. Every pager since fvwm
 * draws the workspaces as SCREENS, and on a cell grid a screen is a filled
 * block two cells wide with its number under it — which is the same two cells
 * per workspace the dots took, on a row that was already there.
 */
static int draw_pager(struct sh_state *sh, int right_x, int x_min, int h)
{
	int ry = (h - 1) / 2;
	int pager_w = sh->nws * 2;

	if (sh->nws <= 0)
		return right_x;

	if (right_x - pager_w - 2 >= x_min) {
		int px = right_x - pager_w;
		sh->pager_hit_x = px;
		for (int i = 0; i < sh->nws; i++) {
			bool active = i == sh->active_ws;
			int fg = active		      ? KT_ACCENT
				 : sh->ws_urgent[i]   ? KT_WARN
				 : sh->ws_occupied[i] ? KT_MID
						      : KT_DIM;
			sh->ws_hit[i] = px + i * 2;
			if (h < 2 || ry + 1 >= h) {
				ktui_draw_text(px + i * 2, ry, 1,
					       active ? ktui_glyph[KT_G_SQUARE]
						      : ktui_glyph[KT_G_DOT],
					       fg, KT_SURFACE, KT_A_NONE);
				continue;
			}
			/*
			 * The screen: ONE cell of the workspace's own colour
			 * over its number, and the second cell of the stride
			 * left as background so four of them read as four
			 * things. Two cells filled looked right for the
			 * ACTIVE one and turned the other three into a single
			 * unbroken dark bar with three digits under it —
			 * photographed. A FILL rather than a block glyph, the
			 * rule the whole panel keeps: an attribute inverts
			 * only the cells a glyph covers.
			 */
			/* The label under it: the name the workspace was
			 * GIVEN when that name FITS — `<desktops>` in rc.xml
			 * names them "1".."4" by default and somebody may have
			 * named one "IM" — and its number when it does not.
			 * Never a truncation: two characters of "Workspace 3"
			 * is "Wo", and four workspaces named that way would
			 * all read the same. */
			char lab[16];
			const char *nm = sh->ws_name[i];
			int lw = nm && *nm ? ktui_utf8_width(nm) : 0;
			if (lw >= 1 && lw <= 2)
				snprintf(lab, sizeof(lab), "%s", nm);
			else
				snprintf(lab, sizeof(lab), "%d", i + 1);
			lw = ktui_utf8_width(lab);
			if (lw < 1)
				lw = 1;
			if (lw > 2)
				lw = 2;		/* workspace 100 and up */
			/*
			 * HOVER IS THE SHAPE THAT WAS DRAWN, not the stride it
			 * was drawn in. The stride is two cells and the screen
			 * and its number occupy the FIRST of them — the second
			 * is the gap that makes four of them read as four
			 * things — so a two-cell backdrop lit a block twice as
			 * wide as the thing under the pointer and closed the
			 * gap to the workspace beside it. Photographed: one
			 * square, a highlight of two. A workspace whose NAME
			 * is two characters wide does occupy both, and then
			 * the backdrop is two: the width is the label's.
			 */
			int hw = lw;
			/* KT_MID, the same fill every other hover on this bar
			 * takes — and here it is not a preference: an
			 * UNOCCUPIED screen is drawn in KT_DIM, so a KT_DIM
			 * hover behind it is the same colour as the thing it
			 * is meant to light. Measured on the booted ISO: the
			 * top row of a hovered empty workspace was pixel-for-
			 * pixel what it had been. The screen goes on TOP of
			 * it, so the workspace's own state colour still wins
			 * the cell it occupies. */
			if (hover_ws == i)
				ktui_draw_fill(krect(px + i * 2, ry, hw, 2),
					       KT_MID);
			ktui_draw_fill(krect(px + i * 2, ry, 1, 1), fg);
			/* On the hover fill the number is drawn in the
			 * SURFACE colour: KT_MID on KT_MID is a digit that
			 * disappears the moment the pointer reaches it. */
			ktui_draw_text(px + i * 2, ry + 1, lw, lab,
				       hover_ws == i ? KT_SURFACE
				       : active     ? KT_ACCENT
						    : KT_MID,
				       hover_ws == i ? KT_MID : KT_SURFACE,
				       KT_A_NONE);
		}
		sh->pager_hit_end = px + pager_w;
		sh->ws_hit_x = px;
		sh->ws_hit_end = px + pager_w;
		/* Its own segment: the pager is where you GO, and everything
		 * to the right of it is what the machine is DOING. The gap
		 * between the two was already there; the rule is what makes it
		 * read as a boundary. */
		if (px + pager_w < ktui_w - 1)
			draw_sep(px + pager_w, h);
		return px - 2;
	}

	char ro[32];
	snprintf(ro, sizeof(ro), "%d/%d", sh->active_ws + 1, sh->nws);
	int rw = ktui_utf8_width(ro);
	if (right_x - rw - 2 < x_min)
		return right_x;
	ktui_draw_text(right_x - rw, ry, rw, ro, KT_DIM, KT_SURFACE, KT_A_NONE);
	return right_x - rw - 2;
}

/*
 * The notification area: the tray, then the recording lamps, then the
 * measurable applets, then the clock. Right to left, each recording the span
 * the frame actually drew — an applet with no room records an EMPTY one,
 * because a hit map that outlives what it describes is how a narrow screen
 * ends up muting itself when somebody aims at the clock.
 */
/*
 * A THEMED NAME FOR THE ITEMS WHOSE OWN ARTWORK IS NOT THE PICTURE.
 *
 * The tray resolves an item's IconName and then its Id through libkicon, which
 * consults the atlas first and the alien apps' own PNGs second — so fcitx5,
 * the one item every KDOS session has, came out as its upstream mark: a
 * full-colour 48px penguin holding a paintbrush, sitting between a phosphor
 * network card and a phosphor speaker. It is the correct icon and it is the
 * wrong picture, and it says "Linux" where the question is "what is this cell".
 *
 * So a handful of ids resolve to a themed name FIRST. This is not a licence to
 * restyle other people's marks — an app icon is still drawn untinted and as
 * shipped, which is the rule the icon theme already keeps. It is the narrow
 * case where the item is a SYSTEM function (an input method is a keyboard) and
 * the theme has the right glyph for it.
 */
static const char *tray_themed(const char *id)
{
	static const struct {
		const char *id, *name;
	} A[] = {
		{ "fcitx", "input-keyboard" },
		{ "fcitx5", "input-keyboard" },
		{ "org.fcitx.fcitx5", "input-keyboard" },
		{ "ibus", "input-keyboard" },
	};

	if (!id || !*id)
		return NULL;
	for (size_t i = 0; i < sizeof(A) / sizeof(A[0]); i++)
		if (!strcasecmp(id, A[i].id))
			return A[i].name;
	return NULL;
}

/*
 * WHICH ITEM THE Nth CELL IS. Hiding one breaks the arithmetic every click on
 * this row used — `(cx - hit_x) / 2` was the item's index because the two were
 * the same number — so the drawn order is recorded and the click reads it. A
 * hit map derived twice is how a click lands on the neighbour of what was
 * aimed at.
 */
static int tray_map[SH_MAX_TRAY];
static int tray_nvis;

static int draw_tray(struct sh_state *sh, int right_x, int x_min, int h)
{
	int ntray = sh_tray_count(sh);
	int ry = (h - 1) / 2;
	int shown = 0;

	tray_nvis = 0;
	for (int i = 0; i < ntray; i++)
		if (!in_overflow[W_TRAY] && !tray_hidden(sh_tray_get(sh, i)->id))
			tray_map[shown++] = i;
	tray_nvis = shown;
	if (shown <= 0 || right_x - x_min < shown * 2 + 2)
		return right_x;

	int tx = right_x - shown * 2;
	sh->tray_hit_x = tx;
	for (int k = 0; k < shown; k++) {
		int i = tray_map[k];
		const struct sh_tray_item *it = sh_tray_get(sh, i);
		char cell[8];
		int fg = it->status == SH_TRAY_ATTENTION ? KT_ACCENT
			 : it->status == SH_TRAY_ACTIVE	  ? KT_TEXT
							  : KT_DIM;

		/*
		 * An item's own IconName, when the theme has a picture for it
		 * — and the first letter of its Id when it does not, which is
		 * the design this tray shipped with and is still the fallback.
		 * `IconName` is consulted only through libkicon, which answers
		 * "no" for free rather than by walking a theme.
		 */
		/* 2x2 on a two-row bar: the tray sat on the applet row with
		 * thirty-two pixels of nothing under it, and an item is a
		 * picture with no label — so it is centred on the wing's full
		 * height, which is the same rule the Start button's mark
		 * keeps. libkicon still draws the largest SQUARE that fits, so
		 * this is the same 32x32 picture, not a stretched one. */
		int irows = h > 1 && ry + 1 < h ? 2 : 1;
		/* `k`, not `i`: hover_tray counts the cells that were DRAWN. */
		int tbg = hover_tray == k ? KT_DIM : KT_SURFACE;
		if (tbg != KT_SURFACE)
			ktui_draw_fill(krect(tx, ry, 2, irows), tbg);
		const char *themed = icons_on ? tray_themed(it->id) : NULL;
		int icon = themed ? kicon_slot(themed, 2, irows) : -1;
		if (icon < 0 && icons_on && it->icon[0])
			icon = kicon_slot(it->icon, 2, irows);
		/*
		 * THEN THE ID, LOWERCASED — because the letter fallback is
		 * what a person actually sees and it says nothing.
		 *
		 * fcitx5 is the item every KDOS session has: it publishes no
		 * usable `IconName`, so the tray drew a bare `F` next to the
		 * clock, forever, on a desktop where nothing else is a letter.
		 * The one question it raises — "what is that" — is the one it
		 * cannot answer. An SNI `Id` is conventionally the program's
		 * own name, and fcitx5 installs `fcitx.png` into the hicolor
		 * tree like every other application, so asking for the id as
		 * an icon name resolves it and costs a lookup that already
		 * answers "no" for free.
		 */
		if (icon < 0 && icons_on && it->id[0]) {
			char lower[64];
			size_t n = 0;
			for (; it->id[n] && n < sizeof(lower) - 1; n++)
				lower[n] = (it->id[n] >= 'A' && it->id[n] <= 'Z')
						   ? (char)(it->id[n] - 'A' + 'a')
						   : it->id[n];
			lower[n] = '\0';
			icon = kicon_slot(lower, 2, irows);
		}
		if (icon >= 0) {
			ktui_draw_sprite(krect(tx, ry, 2, irows), icon, fg, tbg);
			tx += 2;
			continue;
		}
		/*
		 * An item whose Id has not been read yet gets a dim
		 * placeholder, never a letter mined from its bus address:
		 * every Qt item registers as
		 * org.kde.StatusNotifierItem-<pid>-<n>, so a KDE-ish login
		 * drew a row of identical 'O's until the properties arrived.
		 */
		if (!it->id[0]) {
			ktui_draw_text(tx, ry, 1, ktui_glyph[KT_G_DOT], KT_DIM,
				       tbg, KT_A_NONE);
			tx += 2;
			continue;
		}
		/* One BYTE, not one codepoint: ids are program names — ascii
		 * in every case that exists — so the fallback is a dot, not a
		 * decoder. */
		snprintf(cell, sizeof(cell), "%c",
			 (unsigned char)it->id[0] < 0x80 ? it->id[0] : '.');
		if (cell[0] >= 'a' && cell[0] <= 'z')
			cell[0] = (char)(cell[0] - 'a' + 'A');
		ktui_draw_text(tx, ry, 1, cell, fg, tbg,
			       it->status == SH_TRAY_ATTENTION ? KT_A_REVERSE
							       : KT_A_NONE);
		tx += 2;
	}
	sh->tray_hit_end = tx;
	return sh->tray_hit_x - 1;
}

/* ── the meters, as a picture ──────────────────────────────────────────────
 *
 * FIVE METERS, EACH A DESCRIPTOR, so the set is data rather than five copies
 * of a drawing routine. `panel.conf`'s `meters = cpu ram net` names them and
 * the ladder drops them from the RIGHT when the bar is narrow — the order in
 * the file is the order of importance, which is the same promise the widget
 * list already makes.
 */
static const struct mdesc {
	const char *key;	/* what panel.conf calls it              */
	const char *label;
	int cells;		/* how much of the bar it wants          */
	int pct;		/* a percentage: the scale is fixed 0-100 */
	const struct meter *a;	/* the series, or the upper half of a pair */
	const struct meter *b;	/* the lower half, mirrored, or NULL      */
	const char *up, *dn;	/* what the two halves are called         */
} MDESC[MT_N] = {
	{ "cpu",    "CPU", 4, 1, &met_cpu,  NULL,     NULL, NULL },
	{ "ram",    "RAM", 4, 1, &met_ram,  NULL,     NULL, NULL },
	{ "disk",   "SSD", 4, 1, &met_disk, NULL,     NULL, NULL },
	{ "net",    "NET", 7, 0, &met_rx,   &met_tx,  "\xe2\x86\x93", "\xe2\x86\x91" },
	{ "diskio", "I/O", 7, 0, &met_rd,   &met_wr,  "\xe2\x86\x93", "\xe2\x86\x91" },
};

static int meter_by_key(const char *name)
{
	for (int i = 0; i < MT_N; i++)
		if (!strcmp(MDESC[i].key, name))
			return i;
	return -1;
}

/*
 * One kept scale per mirrored meter, and it is kept because the axis has
 * HYSTERESIS: kpr_scale_step() decides the next rung from the last one, so the
 * last one has to survive the frame. A mirrored pair shares one entry — the
 * two series are drawn about a midline and an axis taken from either alone
 * would move under the other one's data.
 */
static double met_scale[MT_N];

static void fmt_rate(char *out, size_t n, double bytes_per_s)
{
	if (bytes_per_s >= 1024.0 * 1024.0)
		snprintf(out, n, "%.1fM", bytes_per_s / (1024.0 * 1024.0));
	else if (bytes_per_s >= 1024.0)
		snprintf(out, n, "%.0fk", bytes_per_s / 1024.0);
	else
		snprintf(out, n, "%.0f", bytes_per_s);
}

/* One meter's reading, as the two strings a band prints. */
static void met_text(int id, char *v1, size_t n1, char *v2, size_t n2)
{
	const struct mdesc *d = &MDESC[id];

	v1[0] = v2[0] = '\0';
	if (d->pct) {
		snprintf(v1, n1, "%d%%", (int)(d->a->shown + 0.5));
		return;
	}
	char r[16];
	fmt_rate(r, sizeof(r), d->a->shown);
	snprintf(v1, n1, "%s%s", d->up, r);
	fmt_rate(r, sizeof(r), d->b->shown);
	snprintf(v2, n2, "%s%s", d->dn, r);
}

/* ── the glyph fallback ───────────────────────────────────────────────── */

/* One meter: `CPU ▁▂▃▅█ 42%`. Returns the width it drew. */
static int draw_meter(int x, int row, const char *label,
		      const struct meter *m, int spark_w, double vmax,
		      const char *value, int lit)
{
	int n = 0;
	const double *series = meter_series(m, &n);
	int lw = ktui_utf8_width(label);
	int vw = ktui_utf8_width(value);

	/* KT_MID, not KT_DIM. `dim` is a FILL — 1.63:1 against the backdrop —
	 * and every label drawn in it on this desktop was something nobody
	 * could read; KT_MID is the derived readable muted colour and is what
	 * every text role goes through. */
	ktui_draw_text(x, row, lw, label, lit ? KT_WARN : KT_MID, KT_SURFACE,
		       KT_A_NONE);
	/* The chart's own background is the bar's, so an empty history reads
	 * as a flat baseline rather than as a hole. */
	ktui_draw_fill(krect(x + lw + 1, row, spark_w, 1), KT_SURFACE);
	if (n > 0)
		ktui_sparkline(krect(x + lw + 1, row, spark_w, 1), series, n,
			       vmax, KT_SURFACE);
	ktui_draw_text(x + lw + 1 + spark_w + 1, row, vw, value,
		       lit ? KT_WARN : KT_TEXT, KT_SURFACE, KT_A_NONE);
	return lw + 1 + spark_w + 1 + vw;
}

/* ── the pixel tile ───────────────────────────────────────────────────── */

/*
 * What the picture depends on.
 *
 * The samples are quantised on the way in — a chart redrawn because the third
 * decimal of a byte rate moved is a raster per frame — and the accent is NOT
 * in here: `kdos theme` drops every tile through kch_tile_reset(), which is the
 * same mechanism kicon_retint() uses and one place rather than two.
 */
static uint64_t met_hash(void)
{
	uint64_t hv = 1469598103934665603ull;

	/* The sample NUMBER, because the gridlines move with it and nothing
	 * else on a flat band does. This is what re-rasters an idle chart. */
	hv = (hv ^ met_seq) * 1099511628211ull;
	for (int k = 0; k < nmeters_sel; k++) {
		const struct mdesc *d = &MDESC[meters_sel[k]];
		const struct meter *ms[2] = { d->a, d->b };
		hv = (hv ^ (uint64_t)(unsigned)meters_sel[k]) *
		     1099511628211ull;
		hv = (hv ^ (uint64_t)(met_scale[meters_sel[k]] / 1024.0)) *
		     1099511628211ull;
		for (int q = 0; q < 2 && ms[q]; q++) {
			int n = 0;
			const double *v = meter_series(ms[q], &n);
			for (int i = 0; i < n; i++) {
				uint64_t g = (uint64_t)(v[i] * 4.0 + 0.5);
				hv = (hv ^ g) * 1099511628211ull;
			}
			hv = (hv ^ (uint64_t)(ms[q]->shown * 4.0 + 0.5)) *
			     1099511628211ull;
		}
	}
	return hv;
}

/*
 * ONE AREA CHART, and every part of this is the difference between a chart
 * that reads and one that flickers.
 *
 * A FILLED AREA UNDER A LINE, not a row of independent bars. At one sample per
 * pixel a bar chart is grass: every column is its own object and the eye has
 * nothing to follow. The fill is the same colour at a third of its weight and
 * the line rides on top of it, which is what every system monitor of the last
 * twenty years draws and is why theirs look calm on exactly this data.
 *
 * THE PLOTTED SERIES IS SMOOTHED, the printed number is not. A one-second CPU
 * sample genuinely swings between 4% and 60% on an idle desktop; a three-point
 * mean takes the hash off the line while keeping every real excursion, because
 * a spike that lasts one second still moves a three-point mean by a third of
 * its height. Smoothing the NUMBER instead would be lying about the instant.
 *
 * THE SCALE IS THE CALLER'S. A chart that autoscales to its own window
 * redraws a moving axis under stationary data — see kpr_scale_step().
 *
 * THE TRACE IS CONTINUOUS, INCLUDING AT ZERO, and that is the other half of
 * what "it goes back and forth one pixel" was. A sample worth less than a pixel
 * is drawn as one — a non-zero sample is never nothing — so a link carrying a
 * few hundred bytes a second was a row of isolated dots blinking in and out as
 * they scrolled, with nothing joining them. Every column now puts the line down
 * even when the bar is empty, so an idle meter reads as a live trace lying on
 * its baseline and a dribble is a bump on it.
 */
static void met_graph(KCellCanvas *cv, int x, int y, int w, int h,
		      const struct meter *m, double vmax, int slot, int flip)
{
	int n = 0;
	const double *v = meter_series(m, &n);

	if (w <= 0 || h <= 0 || n <= 0 || vmax <= 0)
		return;

	/*
	 * THE TRACE SPANS THE WHOLE BAND FROM THE FIRST SAMPLE.
	 *
	 * Drawing only the samples there are and right-aligning them leaves
	 * the left of the band EMPTY — no fill, no line, not even a baseline —
	 * for as long as the ring takes to fill, which on the seven-cell
	 * network band is over a minute. Worse, nothing MOVES while that is
	 * happening: the newest sample stays pinned to the right edge and the
	 * picture only starts scrolling once the ring is full, so a chart sat
	 * still and then abruptly began to travel. Reported as "the graphs do
	 * not move smoothly".
	 *
	 * Column i is sample `n - w + i`, and an index before the oldest
	 * sample there is takes the oldest one. The trace is therefore
	 * unbroken across the band from the very first frame, and real data
	 * enters at the right and pushes the flat stretch off the left — which
	 * is motion, one pixel per sample, from the first sample onward.
	 */
	int base = n - w;
	int prev = -1;

	for (int i = 0; i < w; i++) {
		int j = base + i;
		if (j < 0)
			j = 0;
		/* Three-point mean, clamped at the ends so the newest sample
		 * is not held back by a neighbour that does not exist yet. */
		double a = v[j];
		double b = j > 0 ? v[j - 1] : a;
		double c = j + 1 < n ? v[j + 1] : a;
		double f = (a + b + c) / 3.0 / vmax;
		if (f > 1)
			f = 1;
		if (f < 0)
			f = 0;

		int bar = (int)(f * h + 0.5);
		if (bar < 1 && a > 0)
			bar = 1;	/* a non-zero sample is never nothing */
		if (bar > h)
			bar = h;

		/* The area, at a third of the colour's weight. */
		if (bar > 0)
			kcell_canvas_fill(cv, x + i, flip ? y : y + h - bar,
					  1, bar, slot, 90);
		/* The line: the top pixel of this column, plus the run down to
		 * the previous column's top so a steep edge is continuous
		 * rather than a stack of disconnected dots. A column with no
		 * bar draws it on the baseline instead of skipping — see the
		 * head. */
		int top = flip ? y + (bar > 0 ? bar - 1 : 0)
			       : y + h - (bar > 0 ? bar : 1);
		int lo = top, hi = top;
		if (prev >= 0) {
			lo = prev < top ? prev : top;
			hi = prev < top ? top : prev;
		}
		kcell_canvas_fill(cv, x + i, lo, 1, hi - lo + 1, slot, 255);
		prev = top;
	}
}

/*
 * The scrolling scale — see MET_GRID. One faint column per ten seconds of
 * samples, keyed to the absolute sample number so the ticks travel left with
 * the data instead of standing still under it. Drawn UNDER the trace, which is
 * what makes it a scale rather than a decoration.
 */
static void met_grid(KCellCanvas *cv, int x, int y, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;

	/* The whole band, like the trace over it: a scale that stopped where
	 * the samples ran out would say the left of the chart was outside
	 * time. Column w-1 is the newest sample, which is `met_seq`. */
	for (int i = 0; i < w; i++) {
		uint64_t abs = met_seq - (uint64_t)(w - 1 - i);

		if (abs % MET_GRID == 0)
			kcell_canvas_fill(cv, x + i, y, 1, h, KT_MID, 60);
	}
}

/*
 * The strip as one tile. Returns the leftmost column it took, or -1 when it
 * declined — a terminal, `icons = no`, no font, no room.
 *
 * IT DEGRADES BY DROPPING METERS FROM THE RIGHT. A fixed width lost to the
 * window list by one column on the shipped 80-column bar and silently fell
 * through to the glyph strip every time, which is a pixel renderer nobody ever
 * sees. The order in `panel.conf` is the order of importance.
 *
 * THE GRAPH FILLS THE WHOLE TILE AND THE TEXT IS DRAWN OVER IT. Stacking a
 * label row above a chart row spent a third of sixty-four pixels on two words
 * and forced the bands wider than the bar could pay for; a chart grows from
 * the bottom, so the top of it is where a reading can sit.
 */
/*
 * `KDOS_PANEL_DEBUG=1` says why a tile declined.
 *
 * A tile that falls back is INVISIBLE as a failure: the glyph strip draws in
 * its place and the bar looks fine, so "the pixel meters never appear" is a
 * report with no evidence attached. One getenv, printed at most once a second.
 */
static int panel_dbg(void)
{
	static int on = -1;

	if (on < 0) {
		const char *e = getenv("KDOS_PANEL_DEBUG");
		on = e && *e && *e != '0';
	}
	return on;
}

static int draw_meters_tile(struct sh_state *sh, int right_x, int x_min,
			    int row, int rows)
{
	int cells = 0, use = 0;

	if (rows < 2 || nmeters_sel <= 0) {
		if (panel_dbg())
			fprintf(stderr, "panel: meters tile: rows=%d nsel=%d\n",
				rows, nmeters_sel);
		return -1;
	}
	/* Widest set that fits, dropping from the end. */
	for (int want = nmeters_sel; want >= 1 && !cells; want--) {
		int tot = 0;
		for (int i = 0; i < want; i++)
			tot += MDESC[meters_sel[i]].cells;
		if (tot <= 16 && right_x - tot >= x_min) {
			cells = tot;
			use = want;
		}
	}
	if (!cells) {
		if (panel_dbg())
			fprintf(stderr, "panel: meters tile: no room "
					"(right_x=%d x_min=%d)\n",
				right_x, x_min);
		return -1;
	}

	int H = rows * kwl_cell_h() * kwl_scale();
	/*
	 * 23%, not 26. A four-cell band is sixty-two pixels wide and the two
	 * strings on its top line are `CPU` and `100%`; at 26% they came to
	 * fifty-eight and touched — `CPU10%`. The reading has to have air
	 * round it or the band reads as one word.
	 */
	int fsz = H * 23 / 100;
	int asc = kcell_canvas_text_ascent(fsz);

	if (H < 16 || fsz < 6) {
		if (panel_dbg())
			fprintf(stderr, "panel: meters tile: H=%d fsz=%d\n", H,
				fsz);
		return -1;
	}

	/* The scales are settled BEFORE the hash, or a rung change would not
	 * be part of the picture the hash describes and the chart would keep
	 * the axis it had. */
	for (int k = 0; k < use; k++) {
		int id = meters_sel[k];
		const struct mdesc *d = &MDESC[id];
		if (d->pct) {
			met_scale[id] = 100.0;
			continue;
		}
		/*
		 * THE AXIS DESCRIBES THE PICTURE, so the peak is taken over
		 * the samples the band is about to DRAW and not over the whole
		 * ring.
		 *
		 * The ring is 256 samples and a band is about a hundred
		 * pixels, so more than half of it is history that has already
		 * scrolled off the left. Scanning all of it hands the axis to
		 * a spike nobody can see: one burst of traffic at login set
		 * this to 16 MB/s and held it there for over a minute while
		 * every sample on screen was under 120 BYTES — a trace pinned
		 * flat on its baseline under a scale that matched nothing in
		 * front of it. Then the spike aged out of the ring, the axis
		 * fell several rungs at once, and the whole chart jumped.
		 * Measured, with `KDOS_PANEL_DEBUG=1`:
		 *
		 *   NET n=146 scale=16000000 shown=27.6 last=0.0
		 *
		 * Windowing it is also what makes the hysteresis behave: an
		 * axis that only shrinks under a third of itself needs the
		 * peak to be able to fall, and a peak over everything ever
		 * sampled barely can.
		 */
		int span = d->cells * kwl_cell_w() * kwl_scale();
		double peak = 0;
		const struct meter *pair[2] = { d->a, d->b };
		for (int q = 0; q < 2 && pair[q]; q++) {
			int n = 0;
			const double *v = meter_series(pair[q], &n);
			int from = n > span ? n - span : 0;
			for (int i = from; i < n; i++)
				if (v[i] > peak)
					peak = v[i];
		}
		met_scale[id] = kpr_scale_step(peak, met_scale[id]);
	}

	/*
	 * `KDOS_PANEL_DEBUG=1` TRACES THE SERIES, NOT THE PICTURE.
	 *
	 * "The graph goes back and forth" is a report about pixels, and pixels
	 * are the last place to look for it: a chart is a sample ring, an axis
	 * and a scale step, and any one of the three can move the trace with
	 * the other two standing still. One line per SAMPLE — not per frame,
	 * or a bar that redraws sixty times a second buries the thing being
	 * looked for — printed here because this is where all three are in
	 * scope and settled.
	 */
	static uint64_t dbg_seq;
	if (panel_dbg() && met_seq != dbg_seq) {
		dbg_seq = met_seq;
		fprintf(stderr, "panel: seq=%llu w=%d",
			(unsigned long long)met_seq, right_x - x_min);
		for (int k = 0; k < use; k++) {
			int id = meters_sel[k];
			const struct meter *m = MDESC[id].a;

			fprintf(stderr, " | %s n=%d scale=%.0f shown=%.1f "
					"last=%.1f",
				MDESC[id].label, m->h.n, met_scale[id],
				m->shown,
				m->h.n ? kpr_hist_at(&m->h, m->h.n - 1) : -1.0);
		}
		fprintf(stderr, "\n");
	}

	int x = right_x - cells;
	/* The set is part of the picture: a tile that went from three meters
	 * to two without the hash noticing would keep drawing three. */
	KCellCanvas *cv = kch_tile_begin(SH_TILE_METERS, cells, rows,
					met_hash() ^ ((uint64_t)use << 56) ^
						((uint64_t)hover_meters << 63));
	if (cv) {
		int W = kcell_canvas_w(cv);
		int cell = W / cells;
		/* Half a cell of air between bands: at two pixels the CPU's
		 * reading and the memory's label touched — `CPU 1%RAM 6%`. */
		int gut = cell / 2;
		int bx = 0;

		for (int k = 0; k < use; k++) {
			int id = meters_sel[k];
			const struct mdesc *d = &MDESC[id];
			int bw = d->cells * cell - gut;
			char v1[24], v2[24];
			int warn = d->pct && d->a->shown > 90;

			met_text(id, v1, sizeof(v1), v2, sizeof(v2));

			/*
			 * THE PLOT AREA IS DRAWN EVEN WHEN THE PLOT IS NOT.
			 *
			 * An area chart at six percent of a sixty-four pixel
			 * band is four pixels along the bottom edge of the
			 * screen, and an idle machine therefore showed a
			 * label, a number and nothing else — the chart was
			 * working and was invisible. Every system monitor
			 * worth copying gives the plot its own faint backdrop
			 * and a baseline, so the BOX is legible at a glance
			 * and the fill inside it is read against something.
			 */
			kcell_canvas_fill(cv, bx, 0, bw, H, KT_DIM,
					  hover_meters ? 110 : 55);
			met_grid(cv, bx, 0, bw, H);
			if (!d->b) {
				/* The baseline goes down BEFORE the trace: at
				 * rest the two sit on the same row, and drawn
				 * afterwards it would paint over the very line
				 * the chart exists to show. Without it a
				 * flat-zero chart and a chart that is not there
				 * look the same. */
				kcell_canvas_fill(cv, bx, H - 1, bw, 1, KT_DIM,
						  255);
				met_graph(cv, bx, 0, bw, H, d->a,
					  met_scale[id], KT_ACCENT, 0);
				kcell_canvas_text(cv, bx, asc, fsz, d->label,
						  warn ? KT_WARN : KT_MID);
				int vw = kcell_canvas_text_width(fsz, v1);
				kcell_canvas_text(cv, bx + bw - vw, asc, fsz,
						  v1,
						  warn ? KT_WARN : KT_TEXT);
			} else {
				/*
				 * MIRRORED, on ONE shared scale: halves that
				 * autoscaled separately would draw a trickle
				 * and a torrent the same height. Received
				 * above in the accent, sent below in the
				 * secondary — two colours because two
				 * directions.
				 */
				int half = H / 2;
				/* The midline first — it is the zero line for
				 * BOTH halves, and the sent trace lies on it
				 * when the link is quiet. */
				kcell_canvas_fill(cv, bx, half, bw, 1, KT_DIM,
						  255);
				met_graph(cv, bx, 0, bw, half, d->a,
					  met_scale[id], KT_ACCENT, 0);
				/* Below the midline, not on it: the sent half's
				 * own baseline is the first row UNDER the
				 * zero line, so a quiet link shows a dim
				 * midline with a trace either side of it
				 * rather than one line wearing two colours. */
				met_graph(cv, bx, half + 1, bw, H - half - 1,
					  d->b, met_scale[id], KT_WARN, 1);
				int uw = kcell_canvas_text_width(fsz, v1);
				int dw = kcell_canvas_text_width(fsz, v2);
				/* Each number over the half it describes. */
				kcell_canvas_text(cv, bx + bw - uw, asc, fsz,
						  v1, KT_ACCENT);
				kcell_canvas_text(cv, bx + bw - dw,
						  half + asc, fsz, v2,
						  KT_WARN);
				/*
				 * AND THE BAND'S NAME, which the mirrored ones
				 * never had. `CPU` and `RAM` said what they
				 * were and the third band was two arrows and
				 * two numbers — so the one meter whose UNIT is
				 * not obvious (bytes a second, of what?) was
				 * the one with nothing to say it. Top left,
				 * where the other two put it; the bottom left
				 * stays empty because that corner belongs to
				 * the sent half's plot.
				 */
				int lw = kcell_canvas_text_width(fsz, d->label);
				if (lw + uw + fsz / 2 <= bw)
					kcell_canvas_text(cv, bx, asc, fsz,
							  d->label, KT_MID);
			}
			bx += d->cells * cell;
		}
		kch_tile_commit(SH_TILE_METERS);
	}

	int slot = kch_tile_slot(SH_TILE_METERS);
	if (slot < 0) {
		if (panel_dbg())
			fprintf(stderr, "panel: meters tile: no slot "
					"(cells=%d rows=%d cv=%p)\n",
				cells, rows, (void *)cv);
		return -1;
	}
	/*
	 * THE TILE IS `rows` TALL AND `row` IS ITS LAST ROW, NOT ITS FIRST.
	 *
	 * `row` is the strip's row — the second one — because that is what the
	 * GLYPH fallback needs: it is one row of text under the applets. A
	 * two-row tile drawn from there hangs off the bottom of the panel, and
	 * ktui_draw_sprite silently drops the cells that are out of bounds —
	 * so the tile rendered its top half into the panel's lower row and
	 * threw the other half away. The visible result was a chart that
	 * worked, was half the height it claimed, and sat in the wrong place;
	 * it took a plot backdrop to see at all.
	 */
	int top = row + 1 - rows;
	if (top < 0)
		top = 0;
	ktui_draw_sprite(krect(x, top, cells, rows), slot, KT_TEXT,
			 KT_SURFACE);
	sh->meter_hit_x = x;
	sh->meter_hit_end = x + cells;
	/*
	 * AND THE ROWS IT COVERS, because the click test used to name one.
	 *
	 * `row` is the strip's row for the GLYPH fallback, which is one row of
	 * text; the tile is `rows` tall and starts above it. The hit test asked
	 * for `cy == row`, so the top half of every chart — the whole of the
	 * CPU and RAM readings and the received half of the network — was dead:
	 * a click there fell through to the applet walk, matched nothing, and
	 * did nothing at all. Aiming at the middle of a chart is what people
	 * do.
	 */
	meter_row0 = top;
	meter_rows = rows;
	return x;
}

/*
 * Returns the leftmost column the strip took, or `wing_x` when it drew nothing
 * — which is what the window list clamps itself against.
 *
 * TWO RIGHT EDGES, AND CONFUSING THEM PAINTS OVER THE APPLETS. `row_x` is
 * where the clock's own column starts, which is all the GLYPH strip needs: it
 * lives on the second row, where the only other thing is the date under the
 * time. The TILE is two rows tall, so its right edge has to be `wing_x` — the
 * left edge of the leftmost applet, where the right-to-left walk stopped —
 * because everything between there and the clock is battery, volume, network,
 * tray and pager, sitting on the row a two-row tile would cover.
 */
static int draw_meters(struct sh_state *sh, int wing_x, int row_x, int x_min,
		       int row, int rows)
{
	if (row < 0 || row >= ktui_h || !met_cpu.have || nmeters_sel <= 0)
		return wing_x;

	/* Pixels first. The glyph strip below is the fallback and stays
	 * first-class: it is what a tty, `icons = no` and a full sprite table
	 * all draw. */
	int tx = draw_meters_tile(sh, wing_x, x_min, row, rows);
	if (tx >= 0)
		return tx;

	/* Widest first, then narrower, then fewer meters — and never a partial
	 * one, because half a chart is a chart that lies about its window. */
	int spark_w = 0, use = 0;
	for (int want = nmeters_sel; want >= 1 && !spark_w; want--) {
		for (int sw = 10; sw >= 4; sw -= 2) {
			int total = 0;
			for (int i = 0; i < want; i++) {
				char v1[24], v2[24];
				met_text(meters_sel[i], v1, sizeof(v1), v2,
					 sizeof(v2));
				total += ktui_utf8_width(
						 MDESC[meters_sel[i]].label) +
					 1 + sw + 1 + ktui_utf8_width(v1) + 2;
			}
			if (row_x - total >= x_min) {
				spark_w = sw;
				use = want;
				break;
			}
		}
	}
	if (!spark_w)
		return wing_x;

	int total = 0;
	for (int i = 0; i < use; i++) {
		char v1[24], v2[24];
		met_text(meters_sel[i], v1, sizeof(v1), v2, sizeof(v2));
		total += ktui_utf8_width(MDESC[meters_sel[i]].label) + 1 +
			 spark_w + 1 + ktui_utf8_width(v1) + 2;
	}
	int x = row_x - total;

	sh->meter_hit_x = x;
	meter_row0 = row;
	meter_rows = 1;
	int cur = x;
	for (int i = 0; i < use; i++) {
		int id = meters_sel[i];
		char v1[24], v2[24];
		met_text(id, v1, sizeof(v1), v2, sizeof(v2));
		cur += draw_meter(cur, row, MDESC[id].label, MDESC[id].a,
				  spark_w,
				  MDESC[id].pct ? 100.0 : met_scale[id], v1,
				  MDESC[id].pct && MDESC[id].a->shown > 90) +
		       2;
	}
	sh->meter_hit_end = cur;
	return x;
}

/*
 * The recording lamps, and they are CONTROLS now.
 *
 * The panel already knew which application was holding the microphone; what it
 * could not do was anything about it, and an indicator that cannot be acted on
 * is one people learn not to look at. A click on ●MIC mutes every capture
 * device on the machine and a second click puts back what was there.
 *
 * The microphone takes the secondary colour; the camera and a screen share
 * take the fill as well — being seen and being watched outrank being heard.
 */
static int draw_privacy(struct sh_state *sh, int right_x, int x_min)
{
	static const char *const priv_word[] = { "MIC", "CAM", "SCR" };
	static const int priv_ap[] = { SH_AP_MIC, SH_AP_CAM, SH_AP_CAM };
	static const char *const priv_icon[] = { "microphone-sensitivity-high",
						 "camera-web", "video-display" };

	for (int kind = SH_PRIV_MIC; kind < SH_PRIV_NKIND; kind++) {
		int n = sh_priv_count(sh, kind);
		char nm[SH_PRIV_NAME], label[64], head[32];

		if (n <= 0)
			continue;
		/* Cut by CODEPOINT: this is `application.name`, the app's own
		 * string in whatever script it likes, and a byte cut leaves
		 * half a sequence for ktui_utf8_width to measure. */
		sh_utf8_trunc(nm, sizeof(nm), sh_priv_name(sh, kind) ?: "?",
			      n > 1 ? 8 : 12);
		/* The word on top and WHO on the line under it: the lamp is
		 * the alarm and the application's name is the answer, and one
		 * row had to spell both across the widest applet on the bar. */
		if (n > 1)
			snprintf(head, sizeof(head), "%s%s %d",
				 ktui_glyph[KT_G_BULLET], priv_word[kind], n);
		else
			snprintf(head, sizeof(head), "%s%s",
				 ktui_glyph[KT_G_BULLET], priv_word[kind]);
		snprintf(label, sizeof(label), "%s", nm);

		if (kind == SH_PRIV_MIC) {
			int muted = sh_mic_muted();
			if (!applet2(sh, SH_AP_MIC, &right_x, x_min,
				     muted ? "microphone-sensitivity-muted"
					   : priv_icon[kind],
				     head, label, muted ? KT_MID : KT_WARN,
				     KT_MID, KT_SURFACE))
				break;
			/* FILLED, for the camera and the screen share: being
			 * seen and being watched outrank being heard, and a
			 * fill is the emphasis a cell grid has — never
			 * KT_A_REVERSE, which lights only the cells a glyph
			 * covers and leaves holes between the words. */
		} else if (!applet2(sh, priv_ap[kind], &right_x, x_min,
				    priv_icon[kind], head, label, KT_SURFACE,
				    KT_SURFACE, KT_WARN)) {
			break;
		}
	}
	return right_x;
}

/* ── what is behind the chevron ────────────────────────────────────────── */

static void ov_push(const char *key, const char *icon, const char *label,
		    const char *detail, int warn)
{
	if (nov >= OV_MAX)
		return;
	struct ovitem *o = &ov[nov++];

	memset(o, 0, sizeof(*o));
	snprintf(o->key, sizeof(o->key), "%s", key);
	snprintf(o->icon, sizeof(o->icon), "%s", icon);
	snprintf(o->label, sizeof(o->label), "%s", label);
	snprintf(o->detail, sizeof(o->detail), "%s", detail ? detail : "");
	o->warn = warn;
	if (warn)
		ov_warn = 1;
}

/*
 * The hidden items, rebuilt once per frame from the SAME numbers the bar draws
 * — never from a second reading of /proc. A chevron that said `2` while the
 * popup listed three things would be worse than no chevron.
 *
 * A hidden widget with nothing to say contributes nothing, exactly as it would
 * draw nothing on the bar: the point of the chevron is that IT does not come
 * and go, not that its contents cannot.
 */
static void build_overflow(struct sh_state *sh)
{
	char buf[192];

	nov = 0;
	ov_warn = 0;

	if (in_overflow[W_STUTTER] && panel_stutter > 0) {
		snprintf(buf, sizeof(buf),
			 "%d frame%s dropped in the last 10 seconds",
			 panel_stutter, panel_stutter == 1 ? "" : "s");
		ov_push("stutter", "dialog-warning", "Stutter", buf, 1);
	}
	if (in_overflow[W_RESTART] && panel_restarts > 0) {
		snprintf(buf, sizeof(buf),
			 "%d program%s still using files an upgrade replaced",
			 panel_restarts, panel_restarts == 1 ? "" : "s");
		ov_push("restarts", "system-reboot", "Restarts", buf, 1);
	}
	if (in_overflow[W_CLIP] && panel_clip > 0) {
		snprintf(buf, sizeof(buf), "%d entr%s kept", panel_clip,
			 panel_clip == 1 ? "y" : "ies");
		ov_push("clip", "edit-paste", "Clipboard", buf, 0);
	}
	if (in_overflow[W_MEDIA] && panel_media > 0) {
		snprintf(buf, sizeof(buf), "%d attached, %d mounted",
			 panel_media, panel_media_mounted);
		ov_push("media", "drive-removable-media", "Removable media",
			buf, 0);
	}
	if (in_overflow[W_NOTIFY] && notify_unseen >= 0) {
		snprintf(buf, sizeof(buf), "%d new%s", notify_unseen,
			 notify_dnd ? " - Do Not Disturb is on" : "");
		ov_push("notify", notify_dnd ? "weather-clear-night"
					     : "dialog-information",
			"Notifications", buf, 0);
	}
	if (in_overflow[W_CPU] && panel_cpu >= 0) {
		snprintf(buf, sizeof(buf), "%d%% of the machine", panel_cpu);
		ov_push("cpu", "speedometer", "Processor", buf, panel_cpu > 90);
	}
	if (in_overflow[W_MPRIS] && sh_mpris_have(sh->mpris)) {
		snprintf(buf, sizeof(buf), "%s", sh_mpris_title(sh->mpris));
		ov_push("mpris", sh_mpris_playing(sh->mpris)
					 ? "media-playback-start"
					 : "media-playback-pause",
			"Playing", buf, 0);
	}
	if (in_overflow[W_PRIVACY]) {
		int mic = sh_priv_count(sh, SH_PRIV_MIC);
		int cam = sh_priv_count(sh, SH_PRIV_CAM);

		if (mic > 0) {
			snprintf(buf, sizeof(buf), "%s is recording",
				 sh_priv_name(sh, SH_PRIV_MIC));
			ov_push("mic", "audio-input-microphone", "Microphone",
				buf, 1);
		}
		if (cam > 0) {
			snprintf(buf, sizeof(buf), "%s is using the camera",
				 sh_priv_name(sh, SH_PRIV_CAM));
			ov_push("camera", "camera-web", "Camera", buf, 1);
		}
	}

	/*
	 * Tray items, one row each — the ones `tray_hide` names, plus every
	 * one of them when the whole tray is in the overflow.
	 */
	int ntray = sh_tray_count(sh);
	for (int i = 0; i < ntray; i++) {
		const struct sh_tray_item *it = sh_tray_get(sh, i);
		int hide = in_overflow[W_TRAY] || tray_hidden(it->id);

		if (!hide || nov >= OV_MAX)
			continue;
		struct ovitem *o = &ov[nov++];
		memset(o, 0, sizeof(*o));
		snprintf(o->key, sizeof(o->key), "tray");
		/* The THEMED name first, then the item's own, then `unknown` —
		 * draw_tray's order exactly, so the popup and the bar cannot
		 * show two different pictures of one item. (fcitx5's own
		 * IconName is `input-keyboard-symbolic`, which the vendored
		 * Papirus does not carry at all: the row would have had a hole
		 * where its picture goes.) `unknown` is in the atlas;
		 * `application-x-executable`, the obvious guess, is not. */
		const char *ti = tray_themed(it->id);

		if (!ti)
			ti = it->icon[0] ? it->icon : NULL;
		snprintf(o->icon, sizeof(o->icon), "%s", ti ? ti : "unknown");
		snprintf(o->label, sizeof(o->label), "%s",
			 it->title[0] ? it->title : it->id);
		/*
		 * AN ITEM WHOSE ONLY VERB IS A MENU WE CANNOT DRAW SAYS SO.
		 * `ItemIsMenu` means Activate is "show my menu", the menu is
		 * com.canonical.dbusmenu, and this tray does not render it —
		 * so a click on that item does nothing, silently, which is
		 * exactly the report this whole change came from.
		 */
		snprintf(o->detail, sizeof(o->detail), "%s",
			 it->is_menu ? "its menu is dbusmenu - this tray does "
				       "not draw one"
				     : "tray item - click to activate");
		snprintf(o->service, sizeof(o->service), "%s", it->service);
		snprintf(o->path, sizeof(o->path), "%s", it->path);
		if (it->status == SH_TRAY_ATTENTION)
			o->warn = ov_warn = 1;
	}
}

/*
 * THE POPUP IS A DUMB RENDERER AND THIS IS WHAT IT RENDERS.
 *
 * kdos-status is a separate process, so it either re-derives every reading
 * (a second implementation of what "3 restarts" means, in a second program,
 * asking /proc again at the moment somebody clicked) or it is handed the list
 * the panel already has. One line per item, tab separated, rewritten each time
 * the popup is opened — the same "the owner publishes, the front end draws"
 * split kdos-notify and kdos-clip keep.
 */
static void write_overflow(void)
{
	const char *run = getenv("XDG_RUNTIME_DIR");
	char path[256], tmp[288];
	FILE *f;

	if (!run || !*run)
		return;
	snprintf(path, sizeof(path), "%s/kdos-panel.overflow", run);
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	f = fopen(tmp, "w");
	if (!f)
		return;
	for (int i = 0; i < nov; i++)
		fprintf(f, "%s\t%s\t%s\t%s\t%d\t%s\t%s\n", ov[i].key, ov[i].icon,
			ov[i].label, ov[i].detail, ov[i].warn, ov[i].service,
			ov[i].path);
	if (fflush(f) != 0 || fclose(f) != 0) {
		remove(tmp);
		return;
	}
	if (rename(tmp, path) != 0)
		remove(tmp);
}

/*
 * THE CHEVRON, AND IT IS DRAWN WHETHER OR NOT ANYTHING IS BEHIND IT — that is
 * the whole point of it. A cell that came and went would be the same movement
 * the widgets it replaced were causing.
 *
 * TWO COLUMNS AND NO GAP, exactly like a tray item, which is what it is next
 * to. An applet tile would be four with its gap, and four columns is the NET
 * chart on an eighty-column bar the moment a window is open — measured on the
 * booted ISO: the strip needs fifteen and had thirteen. Two is precisely what
 * hiding fcitx5's tray item gave back, so the wing is the width it always was.
 *
 * NO COUNT UNDER IT. A digit is one cell wide and the box is two, so it would
 * sit half a cell left of the picture above it — which is the misalignment the
 * three-cell applet tile exists to avoid, and there is no third column here to
 * centre it in. The colour carries the state (something hidden, something
 * wanting attention), the tooltip names the first two items, and the popup has
 * the list.
 *
 * It is skipped only when the overflow is switched off entirely — no widget
 * assigned and no tray id hidden — because then it would be a control for
 * nothing.
 */
static int draw_more(struct sh_state *sh, int right_x, int x_min, int h)
{
	int any = ntray_hide > 0;

	sh->ap_x[SH_AP_MORE] = sh->ap_end[SH_AP_MORE] = 0;
	for (int i = 0; i < W_N && !any; i++)
		any = in_overflow[i];
	if (!any || right_x - x_min < 3)
		return right_x;

	int ry = (h - 1) / 2;
	int irows = h > 1 && ry + 1 < h ? 2 : 1;
	int x = right_x - 2;
	int fg = ov_warn ? KT_WARN : nov > 0 ? KT_TEXT : KT_MID;
	int bg = hover_ap == SH_AP_MORE ? KT_DIM : KT_SURFACE;

	if (bg != KT_SURFACE)
		ktui_draw_fill(krect(x, ry, 2, irows), bg);
	/* The chevron points the way the popup opens: up from a bottom bar,
	 * down from a top one. */
	int slot = icons_on ? kicon_slot(panel_top ? "pan-down" : "pan-up", 2,
					 irows)
			    : -1;
	if (slot >= 0)
		ktui_draw_sprite(krect(x, ry, 2, irows), slot, fg, bg);
	else
		ktui_draw_text(x, ry, 2,
			       ktui_glyph[panel_top ? KT_G_DOWN : KT_G_UP], fg,
			       bg, KT_A_NONE);
	sh->ap_x[SH_AP_MORE] = x;
	sh->ap_end[SH_AP_MORE] = x + 2;
	/*
	 * ONE COLUMN BACK, AND IT IS NOT A GAP FOR TIDINESS — the pager draws
	 * its own SEPARATOR at whatever `right_x` it was handed (`draw_sep(px +
	 * pager_w)` is exactly the column it was given), which is this bar's
	 * convention for the widget on the left of a boundary. Returning `x`
	 * put that rule straight through the chevron's left cell: the sprite
	 * drew all four of its cells and the separator then overwrote the two
	 * on the left, so the picture came up as its own right half. Measured
	 * on the booted ISO — ink in one cell column where a 2x2 sprite had
	 * put it in two. The tray reserves the same column and always has.
	 */
	return x - 1;
}

static void draw_taskbar(struct sh_state *sh)
{
	int w = ktui_w, h = ktui_h;

	clear_hits(sh);
	if (w < 20 || h < 1)
		return;

	build_chips(sh);
	/* Before the passes: the chevron's own label is the COUNT, so the list
	 * has to exist before anything is laid out. */
	build_overflow(sh);

	/* ── measurements, once per frame, before any layout pass ── */
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);

	int muted = 0, vol = sh_volume_get(&muted);
	char iface[32];
	int wifi = 0;
	int up = net_state(iface, sizeof(iface), &wifi);

	char clock[64], date[64];
	if (strftime(clock, sizeof(clock), clock_fmt, &tm) == 0)
		snprintf(clock, sizeof(clock), "%02d:%02d", tm.tm_hour,
			 tm.tm_min);
	if (strftime(date, sizeof(date), "%a %d %b", &tm) == 0)
		date[0] = '\0';

	applet_row = (h - 1) / 2;

	/*
	 * THREE PASSES, AND THE ORDER IS THE PRIORITY.
	 *
	 * `pass 0` is the whole bar. `pass 1` drops the METERS. `pass 2` also
	 * collapses the Start button to its mark. `pass 3` also drops the
	 * quick-launch row.
	 *
	 * It used to be two passes with the meters inside both, so a single
	 * window that did not fit took the Start button's word AND the entire
	 * quick-launch row with it while a fifteen-cell chart stayed — which
	 * is exactly backwards. Each rung gives up the least useful thing
	 * left: a chart, then a word whose icon still says the same thing,
	 * then a row of shortcuts that are all in the menu anyway. Nothing
	 * here may drop a window BUTTON — that is what icon mode and the `+N`
	 * cell are for, and it is why there are four rungs rather than two.
	 */
	for (int pass = 0; pass < 4; pass++) {
		int meters_on = pass == 0;
		int compact = pass >= 2;	/* the Start button's word */
		int show_favs = pass < 3;	/* the quick-launch row */
		clear_hits(sh);
		ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);

		/* ── the left wing ── */
		int x = draw_start(sh, h, compact);
		draw_sep(x, h);
		x += 2;
		if (show_favs)
			x = draw_quicklaunch(x, w / 2, h);

		/*
		 * Occupancy is DERIVED: ext-workspace-v1 carries ACTIVE,
		 * URGENT and HIDDEN and no "there are windows here", so the
		 * workspace being SHOWN is occupied exactly when a window is
		 * not minimized — kdos-comp re-reports a view on another
		 * workspace as minimized, which is what makes that true. Right
		 * for every workspace the user has visited and silent about
		 * the rest, which is the honest shape.
		 */
		if (sh->active_ws >= 0 && sh->active_ws < SH_MAX_WS) {
			int live = 0;
			for (int i = 0; i < sh->ntasks; i++)
				if (!sh->tasks[i].minimized)
					live = 1;
			sh->ws_occupied[sh->active_ws] = live;
		}

		/* The floor the right wing may not cross: enough for one
		 * window button, whenever there is a window to put in it. */
		int floor_x = x + (nchips > 0 ? CHIP_MINW_BOTTOM + 1 : 0);

		/* ── the right wing, right to left ──
		 *
		 * Right to left is load-bearing: the right wing has a fixed
		 * width and the window list is what has to give when the
		 * screen is narrow. Laying out left to right and hoping is how
		 * a clock ends up pushed off a 1280-wide display by one long
		 * window title.
		 *
		 * The ORDER is the widget list, walked backwards — the list
		 * reads left to right the way the bar does, and the layout
		 * runs the other way.
		 */
		int right_x = w - 2;

		/* Show desktop: one column at the very edge, every row, past
		 * everything else. XP put it beside the clock and so does
		 * this. */
		sh->show_hit_x = w - 1;
		sh->show_hit_end = w;
		for (int r = 0; r < h; r++)
			ktui_draw_text(w - 1, r, 1, ktui_glyph[KT_G_SHADE],
				       hover_show ? KT_ACCENT : KT_DIM,
				       KT_SURFACE, KT_A_NONE);

		char label[96];
		for (int k = nwidgets - 1; k >= 0; k--) {
			/* Behind the chevron: not drawn here, and already
			 * counted by build_overflow. */
			if (widgets[k] != W_MORE && in_overflow[widgets[k]])
				continue;
			switch (widgets[k]) {
			case W_CLOCK: {
				/* The one thing that must not move, and on a
				 * two-row bar the only item that uses both
				 * rows — the time above the date. */
				int cw = ktui_utf8_width(clock);
				int dw = h > 1 ? ktui_utf8_width(date) : 0;
				int clockw = cw > dw ? cw : dw;

				if (right_x - clockw <= floor_x)
					break;
				right_x -= clockw;
				int cbg = hover_ap == SH_AP_CLOCK ? KT_DIM
								  : KT_SURFACE;
				if (cbg != KT_SURFACE)
					ktui_draw_fill(krect(right_x, applet_row,
							     clockw, h - applet_row),
						       cbg);
				ktui_draw_text(right_x, applet_row, clockw,
					       clock, KT_TEXT, cbg, KT_A_NONE);
				if (dw)
					/* KT_MID for the same reason the
					 * meters' labels take it: this is
					 * TEXT, and `dim` is a fill. */
					ktui_draw_text(right_x, applet_row + 1,
						       clockw, date, KT_MID,
						       cbg, KT_A_NONE);
				sh->ap_x[SH_AP_CLOCK] = right_x;
				sh->ap_end[SH_AP_CLOCK] = right_x + clockw;
				right_x -= 1;
				/* The clock is its own segment — it is the one
				 * thing on this bar that is never a control
				 * for whatever is beside it. */
				if (right_x > 1)
					draw_sep(right_x, h);
				right_x -= 1;
				break;
			}
			case W_BATTERY: {
				if (panel_pct < 0)
					break;
				/* The warning colour only while actually
				 * running down: a battery held at its
				 * threshold on AC is not dying. */
				int bfg = panel_discharging && panel_pct < 15
						  ? KT_ERR
					  : panel_discharging && panel_pct < 30
						  ? KT_WARN
						  : KT_TEXT;
				const char *bi = panel_pct >= 80 ? "battery-full"
						 : panel_pct >= 40
							 ? "battery-good"
						 : panel_pct >= 15
							 ? "battery-low"
							 : "battery-caution";
				char bic[48];
				snprintf(bic, sizeof(bic), "%s%s", bi,
					 panel_charging ? "-charging" : "");
				/* No percent sign: three cells, and the icon
				 * beside it has already said what the number
				 * is about. `100` is the only four-wide one
				 * and it is the one that matters least. */
				snprintf(label, sizeof(label), "%d", panel_pct);
				applet_tile(sh, SH_AP_BATT, &right_x, floor_x,
					    bic, label, bfg, bfg);
				break;
			}
			case W_VOLUME: {
				if (vol < 0)
					break;
				/* The LEVEL under the speaker, not the number:
				 * three cells of bar say "most of the way up"
				 * at a glance, which is the whole question a
				 * volume readout is looked at to answer, and
				 * the exact figure is one click away in the
				 * slider. Muted is the icon's job. */
				char bar[32];
				level_bar(bar, sizeof(bar), muted ? 0 : vol,
					  AP_TILE_W);
				applet_tile(sh, SH_AP_VOL, &right_x, floor_x,
					    muted     ? "audio-volume-muted"
					    : vol >= 66 ? "audio-volume-high"
					    : vol >= 33 ? "audio-volume-medium"
							: "audio-volume-low",
					    bar, muted ? KT_MID : KT_TEXT,
					    muted ? KT_DIM : KT_ACCENT);
				break;
			}
			case W_NET: {
				/* WHAT IT IS CARRYING, in the direction that
				 * is busier. The interface's NAME is in the
				 * popup a click opens: the icon already says
				 * wired or wireless and the rate already says
				 * the link is alive, and `eth0` cost five
				 * cells to say what neither of them needed.
				 * The strip's chart has both directions and
				 * their history; this is the headline. */
				char rate[16];
				double rx = met_rx.shown, tx = met_tx.shown;
				if (!up)
					snprintf(label, sizeof(label), "off");
				else if (rx < 1 && tx < 1)
					snprintf(label, sizeof(label), "%s",
						 ktui_glyph[KT_G_DOT]);
				else {
					fmt_rate(rate, sizeof(rate),
						 rx >= tx ? rx : tx);
					snprintf(label, sizeof(label), "%s%s",
						 ktui_glyph[rx >= tx ? KT_G_DOWN
								     : KT_G_UP],
						 rate);
					/* THE TILE IS THREE CELLS AND THE
					 * READING COMES FIRST. `12k` fills it
					 * on its own, so on a busy link the
					 * arrow is what gives way — the
					 * direction is still under it in the
					 * strip's own two colours, and a
					 * clipped number is a number nobody
					 * can use. */
					if (ktui_utf8_width(label) > AP_TILE_W)
						snprintf(label, sizeof(label),
							 "%s", rate);
				}
				applet_tile(sh, SH_AP_NET, &right_x, floor_x,
					    up ? (wifi ? "network-wireless"
						       : "network-wired")
					       : "network-offline",
					    label, up ? KT_TEXT : KT_MID,
					    KT_MID);
				break;
			}
			case W_CPU:
				/* On a two-row bar the CPU lives in the meters
				 * strip below, WITH its chart, and a second
				 * copy of the same number up here would be two
				 * things to keep in agreement. One row has no
				 * strip, so the applet is what there is. */
				if (h > 1 || panel_cpu < 0)
					break;
				snprintf(label, sizeof(label), "CPU %d%%",
					 panel_cpu);
				applet(sh, SH_AP_CPU, &right_x, floor_x, label,
				       panel_cpu > 90 ? KT_WARN : KT_DIM,
				       KT_A_NONE);
				break;
			case W_MPRIS: {
				if (!sh_mpris_have(sh->mpris))
					break;
				int play = sh_mpris_playing(sh->mpris);
				char t[64];
				sh_utf8_trunc(t, sizeof(t),
					      sh_mpris_title(sh->mpris),
					      AP_WIDE_TEXT);
				snprintf(label, sizeof(label), "%s", t);
				applet2(sh, SH_AP_MPRIS, &right_x, floor_x,
					play ? "media-playback-start"
					     : "media-playback-pause",
					label, play ? "playing" : "paused",
					play ? KT_ACCENT : KT_MID, KT_MID,
					KT_SURFACE);
				break;
			}
			case W_CLIP:
				/* Nothing kept and nothing to say: a cell that
				 * reads `CLIP 0` for the whole session is a
				 * cell that has taught people to stop looking
				 * at that end of the bar. */
				if (panel_clip <= 0)
					break;
				snprintf(label, sizeof(label), "%d", panel_clip);
				applet_tile(sh, SH_AP_CLIP, &right_x, floor_x,
					    "edit-paste", label, KT_MID,
					    KT_MID);
				break;
			case W_MEDIA:
				if (panel_media <= 0)
					break;
				/* The MOUNTED count is the accent one, because
				 * that is the state a person is about to act
				 * on — an unmounted stick is a thing to mount
				 * and a mounted one is a thing to eject. */
				snprintf(label, sizeof(label), "%d",
					 panel_media_mounted ? panel_media_mounted
							     : panel_media);
				applet_tile(sh, SH_AP_MEDIA, &right_x, floor_x,
					    "drive-removable-media", label,
					    panel_media_mounted ? KT_ACCENT
								: KT_MID,
					    panel_media_mounted ? KT_ACCENT
								: KT_MID);
				break;
			case W_RESTART:
				if (panel_restarts <= 0)
					break;
				/* A count and what it means: `kdos restarts`
				 * says how many running programs are using a
				 * file that has been replaced under them, and
				 * a bare bullet said only "something". */
				snprintf(label, sizeof(label), "%d",
					 panel_restarts);
				applet_tile(sh, SH_AP_RESTART, &right_x,
					    floor_x, "system-reboot", label,
					    KT_WARN, KT_WARN);
				break;
			case W_NOTIFY: {
				/*
				 * THE BADGE IS THE FEATURE. A history nothing
				 * points at is a history nobody opens, and
				 * this is the one cell on the bar that answers
				 * "what did I miss" — so it is drawn whenever
				 * the daemon is there at all, with the count
				 * of what has arrived since somebody last
				 * looked, a dot when there is nothing new, and
				 * the busy mark while Do Not Disturb is on.
				 */
				if (notify_unseen < 0)
					break;
				if (notify_unseen > 0)
					snprintf(label, sizeof(label), "%d",
						 notify_unseen);
				else
					snprintf(label, sizeof(label), "%s",
						 ktui_glyph[KT_G_DOT]);
				/*
				 * A MOON FOR DO NOT DISTURB, which is what
				 * every phone made since 2015 uses — and,
				 * more to the point, a name the vendored
				 * Papirus actually carries at a size the
				 * atlas keeps. `user-busy` is in Papirus's
				 * `panel/` context, which vendor.py does not
				 * take, so the cell came out EMPTY: the state
				 * that most needs saying was the one saying
				 * nothing. Photographed.
				 */
				applet_tile(sh, SH_AP_NOTIFY, &right_x, floor_x,
					    notify_dnd ? "weather-clear-night"
					    : notify_unseen > 0 ? "mail-unread"
							        : "dialog-information",
					    label,
					    notify_dnd	       ? KT_MID
					    : notify_unseen > 0 ? KT_ACCENT
								: KT_MID,
					    notify_unseen > 0 ? KT_ACCENT
							      : KT_MID);
				break;
			}
			case W_STUTTER:
				if (panel_stutter <= 0)
					break;
				snprintf(label, sizeof(label), "%d",
					 panel_stutter > 999 ? 999
							     : panel_stutter);
				applet_tile(sh, SH_AP_STUTTER, &right_x,
					    floor_x, "dialog-warning", label,
					    KT_WARN, KT_WARN);
				break;
			case W_MORE:
				right_x = draw_more(sh, right_x, floor_x, h);
				break;
			case W_PRIVACY:
				right_x = draw_privacy(sh, right_x, floor_x);
				break;
			case W_TRAY:
				right_x = draw_tray(sh, right_x, floor_x, h);
				break;
			case W_PAGER:
				right_x = draw_pager(sh, right_x, floor_x, h);
				break;
			default:
				break;
			}
		}

		/*
		 * ── the meters strip, on the row under the wing ──
		 *
		 * Its right edge is the clock, which is the one thing on this
		 * bar that must not move: the date sits below the time, and
		 * the strip stops one cell short of it.
		 *
		 * IT IS PART OF THE RIGHT WING, not a free row. It used to be
		 * drawn on row 1 and the window list was then given everything
		 * left of the APPLET row's edge — so with seven windows open
		 * the chips ran straight across the charts and painted over
		 * them: `CPU` gone, `RAM` starting mid-word under a task
		 * button. A status area a window list can overwrite is not a
		 * status area, and no panel that ships behaves that way. The
		 * wing's left edge is now whichever of its two rows reaches
		 * further left, and the chips stop there.
		 *
		 * The floor it is given is the room the window list needs, so
		 * on a narrow bar the strip degrades — fewer meters, shorter
		 * charts, and finally nothing — rather than squeezing the
		 * thing a taskbar exists to show.
		 */
		int wing_left = right_x;
		if (h > 1 && meters_on) {
			/*
			 * ONE RIGHT EDGE NOW, AND IT IS THE WING'S.
			 *
			 * The glyph strip used to be allowed to run under the
			 * applets as far as the clock, because the applets
			 * were one row of text with an empty row beneath
			 * them. They are two-row tiles now — a picture, a
			 * headline and a detail line — so that row belongs to
			 * the network readout and the volume's level bar, and
			 * a strip that reached the clock would draw straight
			 * through both. Both the tile and the fallback stop
			 * where the right-to-left walk stopped.
			 */
			int mr = right_x;
			/*
			 * The room the window list needs, at its LABEL floor —
			 * the same figure the acceptance test below uses, or
			 * the meters take space the chips then reject and the
			 * whole pass is thrown away.
			 */
			/*
			 * THE WINDOW LIST'S FLOOR IS ITS ICON FLOOR, NOT ITS
			 * LABEL ONE — and the difference is whether a 1280
			 * screen ever sees a chart.
			 *
			 * Reserving six cells a window meant that with two
			 * windows open there was no room left for the meters
			 * at eighty columns, so the strip was dropped whole
			 * while every window button kept its NAME. That is
			 * the wrong trade in both directions: the list
			 * degrades to icon mode by itself (three cells, every
			 * button still there, the picture and the state
			 * marker) and a chart is worth more than a word whose
			 * icon is already saying it. Reserve what the list
			 * needs to keep every button, and let the labels be
			 * what gives way.
			 */
			int need = nchips > 0 ? nchips * CHIP_MINW_ICON : 0;
			int cap = (w - x) / 2;	/* never more than half the row */
			if (need > cap)
				need = cap;
			/* `+ 2`, not `+ 1`: avail is `wing_left - x - 2`, so a
			 * floor of `x + need + 1` leaves the list one cell
			 * SHORT of the `need` the acceptance test below then
			 * measures it against. One cell, and it is the
			 * difference between a window button and an empty
			 * row. */
			int ml = draw_meters(sh, right_x, mr, x + need + 2,
					     applet_row + 1, h);
			if (ml < wing_left)
				wing_left = ml;
		}

		/*
		 * The boundary between the window list and the status area —
		 * the one rule the bar most obviously wanted and did not have.
		 *
		 * ONE COLUMN BEFORE the wing, not on it. `wing_left` is the
		 * first column the wing OCCUPIES, so drawing the rule there
		 * put it straight through the meters tile's leftmost cell and
		 * sliced the C and P off `CPU` — photographed. The gap the
		 * window list already leaves is where it goes.
		 */
		if (wing_left > x + 2 && wing_left < w - 1)
			draw_sep(wing_left - 1, h);

		/* ── the window list, in whatever room is left ── */
		int avail = wing_left - x - 2;
		/* Every rung above the last gets to refuse. The last one draws
		 * whatever it has, because a bar with no window buttons at all
		 * is the one outcome none of this may produce. */
		/* The same floor the meters reserved against, or the pass
		 * throws away a layout the strip was measured for. */
		if (pass < 3 && nchips > 0 && avail < nchips * CHIP_MINW_ICON)
			continue;
		draw_chips(sh, x, x + avail, 1, h);
		break;
	}

	ktui_draw_flush();
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

/* Which notification-area applet is at this cell, or -1. */
static int applet_at(const struct sh_state *sh, int cx)
{
	for (int i = 0; i < SH_AP_N; i++)
		if (sh->ap_end[i] > sh->ap_x[i] && cx >= sh->ap_x[i] &&
		    cx < sh->ap_end[i])
			return i;
	return -1;
}

static int in_span(int cx, int a, int b)
{
	return b > a && cx >= a && cx < b;
}

/* The tooltip's own state machine, defined below with the words it draws. */
static int tip_kind, tip_idx;
static int tip_x;		/* the left edge of the thing, in cells */
static int64_t tip_since;
static pid_t tip_pid = -1;
static int tip_shown;
static void tip_kill(void);
static int tip_target(const struct sh_state *sh, int cx, int cy, int *idx,
		      int *x);

/* The pointer moved. Nothing here acts; it only records what is under it, so
 * the next frame can light it. */
static void handle_motion(struct sh_state *sh, int cx, int cy)
{
	sh->hover_start = in_span(cx, sh->start_x, sh->start_end);
	sh->hover_task = chip_at(sh, cx);
	hover_fav = -1;
	for (int i = 0; i < nfavs; i++)
		if (in_span(cx, fav_x[i], fav_end[i]))
			hover_fav = i;
	if (drag_fav >= 0) {
		/* libkwl reports a motion only when the pointer changed CELL,
		 * so arriving here at all is the gesture leaving its icon. */
		drag_moved = 1;
		drag_over = hover_fav;
	}
	/* The strip opens kdos-res, so it is a control and has to look like one
	 * under the hand. Its own rows, for the same reason the click test
	 * needs them. */
	hover_meters = meter_row0 >= 0 && cy >= meter_row0 &&
		       cy < meter_row0 + meter_rows &&
		       in_span(cx, sh->meter_hit_x, sh->meter_hit_end);
	hover_ap = applet_at(sh, cx);
	hover_ws = ws_at(sh, cx);
	hover_tray = in_span(cx, sh->tray_hit_x, sh->tray_hit_end)
			     ? (cx - sh->tray_hit_x) / 2
			     : -1;
	hover_show = in_span(cx, sh->show_hit_x, sh->show_hit_end);

	/*
	 * THE DWELL RESTARTS WHENEVER THE THING CHANGES — including on a leave,
	 * which arrives here as an off-grid x. A tooltip that outlived the item
	 * it describes would be a label on the wrong control, which is worse
	 * than none.
	 */
	int idx = 0, tx = 0;
	int kind = tip_target(sh, cx, cy, &idx, &tx);

	if (kind != tip_kind || idx != tip_idx) {
		tip_kill();
		tip_kind = kind;
		tip_idx = idx;
		tip_x = tx;
		tip_since = panel_now_ms();
	}
}

/* ── tooltips ──────────────────────────────────────────────────────────────
 *
 * HALF THIS BAR IS PICTURES WITH NO WORDS, and every one of them is a control.
 * The notification area is a row of 32-pixel icons with a number under each;
 * the quick-launch strip is icons and nothing else; a window button in icon
 * mode is a picture and a state marker. The only way to learn what any of them
 * did was to click it and find out — which for `Shut Down` on the Start menu's
 * footer is a poor way to be taught.
 *
 * A DWELL, NOT A HOVER. The tip is a separate PROCESS (libktui has one cell
 * buffer, so a second surface is a second program — the rule every popup here
 * already keeps), so a pointer sweeping the bar must not fork thirty of them:
 * it appears after 700 ms of stillness on ONE thing, which is roughly when a
 * hand has stopped because it wants to know rather than because it is passing
 * through.
 */
#define TIP_DELAY_MS 700

enum {
	TT_NONE = 0, TT_START, TT_FAV, TT_CHIP, TT_PLUSN, TT_METERS, TT_AP,
	TT_WS, TT_TRAY, TT_SHOW
};

static void tip_kill(void)
{
	if (tip_pid > 0) {
		kill(tip_pid, SIGTERM);
		waitpid(tip_pid, NULL, 0);
		tip_pid = -1;
	}
	tip_shown = 0;
}

/*
 * A CLICK SPENDS THE DWELL, and killing the tip is not enough to spend it.
 *
 * `tip_kill` clears `tip_shown` because the caller that normally reaches it is
 * a MOTION, which has just moved to another thing and owes it a fresh dwell.
 * A click moves nothing: the pointer is still on the applet, `tip_kind` still
 * names it and `tip_since` is still long past, so the very next frame put the
 * tooltip straight back up — over the Start menu, over the volume slider, over
 * whatever the click had just opened. Photographed on the booted ISO: the
 * whole label and its hint line drawn across the top of the popup.
 *
 * So the dwell is marked SPENT and the next tip has to be earned by moving to
 * something else, which is what `handle_motion` already does for every other
 * reason.
 */
static void tip_spend(void)
{
	tip_kill();
	tip_shown = 1;
}

/* What the pointer is on, and where its left edge is. */
static int tip_target(const struct sh_state *sh, int cx, int cy, int *idx,
		      int *x)
{
	*idx = 0;
	*x = cx;
	if (cx < 0)
		return TT_NONE;
	if (in_span(cx, sh->start_x, sh->start_end)) {
		*x = sh->start_x;
		return TT_START;
	}
	for (int i = 0; i < nfavs; i++)
		if (in_span(cx, fav_x[i], fav_end[i])) {
			*idx = i;
			*x = fav_x[i];
			return TT_FAV;
		}
	if (meter_row0 >= 0 && cy >= meter_row0 &&
	    cy < meter_row0 + meter_rows &&
	    in_span(cx, sh->meter_hit_x, sh->meter_hit_end)) {
		*x = sh->meter_hit_x;
		return TT_METERS;
	}
	int ap = applet_at(sh, cx);
	if (ap >= 0) {
		*idx = ap;
		*x = sh->ap_x[ap];
		return TT_AP;
	}
	int ws = ws_at(sh, cx);
	if (ws >= 0) {
		*idx = ws;
		*x = sh->ws_hit[ws];
		return TT_WS;
	}
	if (in_span(cx, sh->tray_hit_x, sh->tray_hit_end)) {
		int k = (cx - sh->tray_hit_x) / 2;

		if (k >= 0 && k < tray_nvis) {
			*idx = k;
			*x = sh->tray_hit_x + k * 2;
			return TT_TRAY;
		}
	}
	if (in_span(cx, plusn_x, plusn_end)) {
		*x = plusn_x;
		return TT_PLUSN;
	}
	if (in_span(cx, sh->show_hit_x, sh->show_hit_end)) {
		*x = sh->show_hit_x;
		return TT_SHOW;
	}
	int ci = chip_at(sh, cx);
	if (ci >= 0) {
		*idx = ci;
		*x = sh->task_hit_x + (ci - chip_off) * sh->task_cell_w;
		return TT_CHIP;
	}
	return TT_NONE;
}

/*
 * The words. Each is what the thing IS, and under it what it is doing or what
 * a click will do — never both halves saying the same thing, and never a name
 * the picture already carries.
 */
static int tip_text(struct sh_state *sh, int kind, int idx, char *t1, size_t n1,
		    char *t2, size_t n2)
{
	t1[0] = t2[0] = '\0';
	switch (kind) {
	case TT_START:
		snprintf(t1, n1, "%s", "Start");
		snprintf(t2, n2, "%s",
			 "applications, places and power - right-click for the "
			 "window manager");
		return 1;
	case TT_FAV:
		if (idx < 0 || idx >= nfavs)
			return 0;
		snprintf(t1, n1, "%s", favs[idx].name[0] ? favs[idx].name
							 : favs[idx].id);
		snprintf(t2, n2, "%s",
			 "click to launch · drag to reorder · right-click to "
			 "unpin");
		return 1;
	case TT_CHIP: {
		if (idx < 0 || idx >= nchips)
			return 0;
		const struct sh_task *t = &sh->tasks[chips[idx].first];

		snprintf(t1, n1, "%s", chips[idx].label ? chips[idx].label : "");
		if (chips[idx].count > 1)
			snprintf(t2, n2, "%d windows - left lists them, middle "
					 "closes them all", chips[idx].count);
		else
			snprintf(t2, n2, "%s", t->title[0] ? t->title
							   : "no title");
		return 1;
	}
	case TT_PLUSN:
		snprintf(t1, n1, "%s", "More windows");
		snprintf(t2, n2, "%s", "click for the list · the wheel steps");
		return 1;
	case TT_METERS:
		snprintf(t1, n1, "%s", "System meters");
		snprintf(t2, n2, "%s",
			 "left: resources · middle: late frames · right: energy");
		return 1;
	case TT_WS:
		snprintf(t1, n1, "Workspace %d", idx + 1);
		snprintf(t2, n2, "%s",
			 idx < SH_MAX_WS && sh->ws_occupied[idx]
				 ? "has windows - the wheel steps"
				 : "empty - the wheel steps");
		return 1;
	case TT_SHOW:
		snprintf(t1, n1, "%s", "Show desktop");
		snprintf(t2, n2, "%s", "minimise everything");
		return 1;
	case TT_TRAY: {
		if (idx < 0 || idx >= tray_nvis)
			return 0;
		const struct sh_tray_item *it = sh_tray_get(sh, tray_map[idx]);

		snprintf(t1, n1, "%s",
			 it->title[0] ? it->title : (it->id[0] ? it->id
							       : "tray item"));
		/* THE ONE THING A TRAY ITEM CANNOT SAY FOR ITSELF. An item that
		 * declares ItemIsMenu means Activate is "show my menu", the
		 * menu is dbusmenu, and this tray does not draw one — so the
		 * click does nothing and the tip is the only place that can
		 * admit it. */
		snprintf(t2, n2, "%s",
			 it->is_menu ? "its menu is dbusmenu - this tray cannot "
				       "draw it"
				     : "left activates · middle and right are "
				       "its other verbs");
		return 1;
	}
	case TT_AP:
		switch (idx) {
		case SH_AP_CLOCK: {
			time_t now = time(NULL);
			struct tm tm;

			localtime_r(&now, &tm);
			if (strftime(t1, n1, "%A %e %B %Y", &tm) == 0)
				snprintf(t1, n1, "%s", "Clock");
			snprintf(t2, n2, "%s", "click for the calendar");
			return 1;
		}
		case SH_AP_BATT:
			snprintf(t1, n1, "Battery %d%%", panel_pct);
			snprintf(t2, n2, "%s",
				 panel_charging	     ? "charging"
				 : panel_discharging ? "on battery"
						     : "full");
			return 1;
		case SH_AP_VOL: {
			int muted = 0, v = sh_volume_get(&muted);

			snprintf(t1, n1, "Volume %d%%%s", v < 0 ? 0 : v,
				 muted ? " (muted)" : "");
			snprintf(t2, n2, "%s",
				 "click for the slider · wheel to change · "
				 "middle to mute");
			return 1;
		}
		case SH_AP_NET: {
			char iface[32];
			int wifi = 0, up = net_state(iface, sizeof(iface), &wifi);
			char rx[16], tx[16];

			fmt_rate(rx, sizeof(rx), met_rx.shown);
			fmt_rate(tx, sizeof(tx), met_tx.shown);
			if (!up)
				snprintf(t1, n1, "%s", "Network - offline");
			else
				snprintf(t1, n1, "%s - down %sB/s, up %sB/s",
					 iface, rx, tx);
			snprintf(t2, n2, "%s", "click for the networks");
			return 1;
		}
		case SH_AP_NOTIFY:
			snprintf(t1, n1, "Notifications - %d new",
				 notify_unseen < 0 ? 0 : notify_unseen);
			snprintf(t2, n2, "%s",
				 notify_dnd ? "Do Not Disturb is on - middle "
					      "click allows toasts again"
					    : "click for the centre \xc2\xb7 middle "
					      "silences toasts");
			return 1;
		case SH_AP_MORE:
			snprintf(t1, n1, "%s", nov > 0 ? "Hidden status items"
						       : "Status");
			if (nov > 0) {
				/* The first two BY NAME, because the cell
				 * itself carries no count — see draw_more.
				 * A chevron is a control nobody can act on
				 * until something says what is behind it. */
				snprintf(t2, n2, "%s%s%s%s", ov[0].label,
					 nov > 1 ? " · " : "",
					 nov > 1 ? ov[1].label : "",
					 nov > 2 ? " · …" : "");
			} else {
				snprintf(t2, n2, "%s",
					 "nothing hidden - `overflow =` in "
					 "panel.conf decides what lives here");
			}
			return 1;
		case SH_AP_STUTTER:
			snprintf(t1, n1, "%d dropped frames", panel_stutter);
			snprintf(t2, n2, "%s",
				 "click to see who was busy when they were "
				 "late");
			return 1;
		case SH_AP_RESTART:
			snprintf(t1, n1, "%d programs need restarting",
				 panel_restarts);
			snprintf(t2, n2, "%s",
				 "they are still running code an upgrade "
				 "replaced");
			return 1;
		case SH_AP_CLIP:
			snprintf(t1, n1, "Clipboard - %d kept", panel_clip);
			snprintf(t2, n2, "%s", "click to paste something older");
			return 1;
		case SH_AP_MEDIA:
			snprintf(t1, n1, "Removable media - %d attached",
				 panel_media);
			snprintf(t2, n2, "%d mounted - click to mount or eject",
				 panel_media_mounted);
			return 1;
		case SH_AP_MIC:
			snprintf(t1, n1, "%s is recording",
				 sh_priv_name(sh, SH_PRIV_MIC));
			snprintf(t2, n2, "%s", "click to mute every capture "
					       "device");
			return 1;
		case SH_AP_CAM:
			snprintf(t1, n1, "%s is using the camera",
				 sh_priv_name(sh, SH_PRIV_CAM));
			snprintf(t2, n2, "%s", "click for the device manager");
			return 1;
		case SH_AP_MPRIS:
			snprintf(t1, n1, "%s", sh_mpris_title(sh->mpris));
			snprintf(t2, n2, "%s",
				 "click plays or pauses · middle and right "
				 "step the track");
			return 1;
		case SH_AP_CPU:
			snprintf(t1, n1, "Processor %d%%", panel_cpu);
			snprintf(t2, n2, "%s", "click for Resources");
			return 1;
		default:
			return 0;
		}
	default:
		return 0;
	}
}

/* Called once a frame: put the tip up when the dwell is over. */
static void tip_tick(struct sh_state *sh)
{
	char t1[192], t2[256], xs[16], ys[16];

	/* The one child on this bar that is not double-forked — it has to be
	 * killable — so it is also the one that has to be reaped. It exits on
	 * its own timeout when the pointer never moves again. */
	if (tip_pid > 0 && waitpid(tip_pid, NULL, WNOHANG) == tip_pid)
		tip_pid = -1;
	if (tip_shown || tip_kind == TT_NONE)
		return;
	if (panel_now_ms() - tip_since < TIP_DELAY_MS)
		return;
	tip_shown = 1;			/* one attempt per dwell, whatever happens */
	if (!tip_text(sh, tip_kind, tip_idx, t1, sizeof(t1), t2, sizeof(t2)))
		return;
	snprintf(xs, sizeof(xs), "%d", (tip_x > 0 ? tip_x : 0) * kwl_cell_w());
	snprintf(ys, sizeof(ys), "%d", kwl_px_h());
	const char *argv[] = { "kdos-tip", panel_at_flag(), xs, ys, t1, t2,
			       NULL };
	tip_pid = panel_spawn_pid(argv);
}

/* How long until the tip is due, for the poll's own deadline. */
static int tip_due_in(void)
{
	if (tip_shown || tip_kind == TT_NONE)
		return 1 << 30;
	int64_t left = TIP_DELAY_MS - (panel_now_ms() - tip_since);

	return left < 0 ? 0 : (int)left;
}

/*
 * The wheel: volume over the volume applet, SNI Scroll over a tray item (a
 * volume tray icon expects exactly that), workspace stepping over the pager,
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
	if (in_span(cx, sh->tray_hit_x, sh->tray_hit_end)) {
		int k = (cx - sh->tray_hit_x) / 2;

		if (k >= 0 && k < tray_nvis)
			sh_tray_scroll(sh, tray_map[k], up ? 1 : -1);
		return;
	}
	if (in_span(cx, list_x0, list_x1)) {
		chips_wheel(sh, up);
		return;
	}
	if (!in_span(cx, sh->ws_hit_x, sh->ws_hit_end) || sh->nws < 2)
		return;
	int i = sh->active_ws + (up ? -1 : 1);
	if (i < 0)
		i = sh->nws - 1;
	if (i >= sh->nws)
		i = 0;
	sh_activate_workspace(sh, i);
}

/*
 * A click on the notification area.
 *
 * Each applet does the one thing a person aiming at it wants: the volume
 * mutes, the network opens the manager, the clock shows a calendar, the mic
 * lamp mutes every capture device, the restart mark explains itself. LEFT
 * only for the destructive ones — a middle or right click on a readout is a
 * miss, not a request.
 */
static void handle_applet(struct sh_state *sh, int id, int btn)
{
	char xs[16], ys[16];
	const char *at = panel_at_flag();

	/*
	 * WHERE THE POPUP GOES, computed once for every branch below.
	 *
	 * Every one of these used to open CENTRED, as a dialog in the middle
	 * of the screen with no relationship to the thing that was clicked —
	 * which is how a panel applet's own window is not supposed to behave
	 * and is why they read as separate applications rather than as part
	 * of the bar. Anchored bottom-left at the applet's own column, one
	 * bar-height up: layer-shell has no coordinates, so "above this
	 * readout" is an anchor plus a margin.
	 */
	snprintf(xs, sizeof(xs), "%d",
		 (id >= 0 && id < SH_AP_N ? sh->ap_x[id] : 0) * kwl_cell_w());
	snprintf(ys, sizeof(ys), "%d", kwl_px_h());

	if (id == SH_AP_MPRIS && btn != SH_TRAY_BTN_LEFT) {
		/* Middle steps back, right steps forward — the transport a
		 * one-cell widget can honestly carry. */
		sh_mpris_action(sh->mpris, btn == SH_TRAY_BTN_MIDDLE
						  ? "Previous"
						  : "Next");
		return;
	}
	if (btn == SH_TRAY_BTN_RIGHT) {
		/* The right button opens the FULL manager for whatever was
		 * aimed at — the readout's own settings page. A readout that
		 * only ever toggled would leave the machine's real controls
		 * reachable from nowhere but a prompt. */
		switch (id) {
		case SH_AP_VOL:
		case SH_AP_MIC: {
			const char *argv[] = { "kdos-audio", at, xs, ys, NULL };
			panel_spawn(argv);
			return;
		}
		case SH_AP_NET: {
			const char *argv[] = { "kdos-net", at, xs, ys,
				       NULL };
			panel_spawn(argv);
			return;
		}
		case SH_AP_CAM:
		case SH_AP_MEDIA: {
			const char *argv[] = { "kdos-devices", at, xs, ys,
				       NULL };
			panel_spawn(argv);
			return;
		}
		case SH_AP_BATT: {
			const char *argv[] = { "kdos-settings", "--page",
					       "power", NULL };
			panel_spawn(argv);
			return;
		}
		default:
			return;
		}
	}
	if (btn == SH_TRAY_BTN_MIDDLE) {
		/* One gesture, no popup: the volume, the microphone and the
		 * toasts are the three things somebody needs silenced NOW. Do
		 * Not Disturb loses nothing — the notification is still kept,
		 * and the badge beside this cell still counts it. */
		if (id == SH_AP_VOL)
			sh_volume_toggle();
		else if (id == SH_AP_MIC)
			sh_mic_toggle();
		else if (id == SH_AP_NOTIFY) {
			const char *argv[] = { "kdos-notify", "--dnd", NULL };

			panel_spawn(argv);
		}
		return;
	}
	if (btn != SH_TRAY_BTN_LEFT)
		return;
	switch (id) {
	case SH_AP_VOL: {
		/*
		 * THE SLIDER, not a mute toggle. A readout whose only action
		 * silenced the machine is a readout people are afraid to
		 * click; the popup carries the mute switch as a labelled
		 * button beside the bar, where it can be found and undone.
		 * Middle-click is still the one-gesture mute.
		 */
		const char *argv[] = { "kdos-osd", "slider", at, xs, ys, NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_MIC:
		/* Mute everything, and put back what was there on the second
		 * click. An indicator that cannot be acted on is one people
		 * learn to stop looking at. */
		sh_mic_toggle();
		break;
	case SH_AP_CAM: {
		const char *argv[] = { "kdos-devices", at, xs, ys,
				       NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_NET: {
		const char *argv[] = { "kdos-net", at, xs, ys,
				       NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_CLOCK:
	case SH_AP_BATT: {
		const char *argv[] = { "kdos-cal", at, xs, ys,
				       NULL };
		/* The clock and the battery are one popup, so they are one
		 * toggle: clicking either shuts the calendar the other
		 * opened. */
		popup_toggle(SH_AP_CLOCK, argv);
		break;
	}
	case SH_AP_NOTIFY: {
		const char *argv[] = { "kdos-notify", at, xs, ys, NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_STUTTER: {
		/* Straight to the attribution, without the list: this cell IS
		 * the one thing the popup would have been opened to read. */
		const char *argv[] = { "kdos-status", "--open", "stutter", at,
				       xs, ys, NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_MORE: {
		/*
		 * THE CHIP SAYS THAT IT HAPPENED; THE POPUP SAYS WHO — and it
		 * is a popup rather than `foot -e kdos stutter`, which is what
		 * this used to open. A terminal that scrolls a fresh paragraph
		 * every dropped frame is not a report, it is a firehose: it
		 * covered the desktop, it never stopped, and closing it was
		 * the only interaction it offered. kdos-status runs the same
		 * tool INSIDE the popup and lets it be read.
		 */
		write_overflow();
		const char *argv[] = { "kdos-status", at, xs, ys, NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_MPRIS:
		/* Left is play/pause, which is what the cell is for. Middle
		 * and right step the track — see handle_applet's head. */
		sh_mpris_action(sh->mpris, "PlayPause");
		break;
	case SH_AP_CPU: {
		/*
		 * kdos-res rather than a terminal running btop: it is this
		 * distro's own monitor, it is a window rather than a terminal
		 * that covers the desktop, and it is the only thing here that
		 * can name a boxed application rather than its processes.
		 * btop keeps its menu entry and its prompt.
		 */
		const char *argv[] = { "kdos-res", NULL };
		panel_spawn(argv);
		break;
	}
	case SH_AP_CLIP: {
		/* Anchored under itself, above the bar — the same trick the
		 * clock's calendar uses. */
		snprintf(xs, sizeof(xs), "%d", sh->ap_x[id] * kwl_cell_w());
		snprintf(ys, sizeof(ys), "%d", kwl_px_h());
		const char *argv[] = { "kdos-clip", "--pick", at, xs, ys, NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_MEDIA: {
		const char *argv[] = { "kdos-devices", at, xs, ys,
				       NULL };
		popup_toggle(id, argv);
		break;
	}
	case SH_AP_RESTART: {
		const char *argv[] = { "kdos-status", "--open", "restarts", at,
				       xs, ys, NULL };
		popup_toggle(id, argv);
		break;
	}
	default:
		break;
	}
}

/*
 * The Start button.
 *
 * Left opens the menu, right opens the window manager's own root menu (the
 * thing a right-click on the desktop gives, reachable without a bare patch of
 * desktop to aim at), middle opens the run box. All three anchored to the
 * bottom-left corner, above the bar.
 */
/*
 * The button came up. Either the icon that was pressed is under the pointer —
 * a click, so launch it — or it is over another one, which is a reorder.
 * Anything else (released off the row) is a cancelled drag and does nothing,
 * which is what dragging an icon into empty space means everywhere else.
 */
static void fav_release(void)
{
	int from = drag_fav, to = drag_over;

	drag_fav = drag_over = -1;
	drag_moved = 0;
	if (from < 0 || from >= nfavs)
		return;
	if (to < 0 || to == from) {
		if (to == from)
			fav_launch(from);
		return;
	}
	if (sh_fav_move(favs[from].id, to) == 0)
		load_favorites();	/* it re-stats and re-reads the file */
}

static void start_click(int btn)
{
	char ys[16];
	const char *at = panel_at_flag();

	snprintf(ys, sizeof(ys), "%d", kwl_px_h());
	if (btn == SH_TRAY_BTN_MIDDLE) {
		const char *argv[] = { "kdos-run", NULL };
		panel_spawn(argv);
		return;
	}
	if (btn == SH_TRAY_BTN_RIGHT) {
		const char *argv[] = { "kdos-menu", "system", at, "0", ys,
				       NULL };
		panel_spawn(argv);
		return;
	}
	const char *argv[] = { "kdos-start", at, "0", ys, NULL };

	/* The same toggle every other button on this bar keeps, and the same
	 * record: the Start button used to answer "is my menu up" through a
	 * pipe of its own, which could say so and could not CLOSE it. One
	 * mechanism, so the highlight and the toggle cannot disagree. */
	popup_toggle(POPUP_START, argv);
}

static void handle_click(struct sh_state *sh, int cx, int cy, int btn)
{
	/* Whatever this opens goes where the tip is, and a hand that has
	 * clicked has stopped asking what the thing was. */
	tip_spend();
	if (in_span(cx, sh->start_x, sh->start_end)) {
		start_click(btn);
		return;
	}

	/*
	 * The meters strip is the one hit test here that needs the y — it does
	 * not span the bar's full height and the applets sit on the same rows
	 * further right, so without it a click on the CPU chart muted the
	 * volume. It is the ROWS THAT WERE DRAWN, not one row: the tile is two
	 * of them and this used to name the lower, which made the top half of
	 * every chart inert.
	 */
	if (meter_row0 >= 0 && cy >= meter_row0 && cy < meter_row0 + meter_rows &&
	    in_span(cx, sh->meter_hit_x, sh->meter_hit_end)) {
		/*
		 * THREE QUESTIONS A METER RAISES AND CANNOT ANSWER, one per
		 * button. Left is `kdos-res` — which process, and which
		 * APPLICATION. MIDDLE is
		 * `kdos stutter`, which is the only thing on this machine that
		 * can say why a frame was late and who was busy when it was;
		 * it had no surface at all and was reachable only by knowing
		 * the subcommand's name. RIGHT is `kdos-energy`, the same for
		 * the per-app energy share. Both of those are real features of
		 * this distro that the desktop never pointed at.
		 */
		char mx[16], my[16];

		snprintf(mx, sizeof(mx), "%d", sh->meter_hit_x * kwl_cell_w());
		snprintf(my, sizeof(my), "%d", kwl_px_h());
		if (btn == SH_TRAY_BTN_MIDDLE) {
			/* In a POPUP, not a terminal — see SH_AP_STUTTER. */
			const char *argv[] = { "kdos-status", "--open", "stutter",
					       panel_at_flag(), mx, my, NULL };
			panel_spawn(argv);
		} else if (btn == SH_TRAY_BTN_RIGHT) {
			const char *argv[] = { "kdos-status", "--open", "energy",
					       panel_at_flag(), mx, my, NULL };
			panel_spawn(argv);
		} else {
			const char *argv[] = { "foot", "-e", "btop", NULL };
			panel_spawn(argv);
		}
		return;
	}

	int ap = applet_at(sh, cx);
	if (ap >= 0) {
		handle_applet(sh, ap, btn);
		return;
	}

	if (in_span(cx, sh->tray_hit_x, sh->tray_hit_end)) {
		int k = (cx - sh->tray_hit_x) / 2;
		/* The item is told where the pointer was in PIXELS: an app
		 * that pops a menu at the cursor gets the cursor, and one that
		 * ignores the argument loses nothing. */
		if (k >= 0 && k < tray_nvis)
			sh_tray_activate(sh, tray_map[k], btn,
					 cx * kwl_cell_w(), kwl_cell_h());
		return;
	}

	int ci = chip_at(sh, cx);
	if (ci >= 0) {
		chip_click(sh, ci, btn);
		return;
	}

	/*
	 * A RIGHT CLICK ON A PINNED ICON UNPINS IT, and it is the only way to.
	 * Pinning is reachable from the window menu, so the reverse has to be
	 * reachable from the pinned icon or the row is a one-way door. No
	 * confirmation: it puts back a line in a text file and the same menu
	 * pins it again.
	 */
	if (btn == SH_TRAY_BTN_RIGHT) {
		for (int i = 0; i < nfavs; i++)
			if (in_span(cx, fav_x[i], fav_end[i])) {
				sh_fav_set(favs[i].id, 0);
				load_favorites();
				hover_fav = -1;
				return;
			}
	}

	if (btn != SH_TRAY_BTN_LEFT)
		return;

	if (in_span(cx, plusn_x, plusn_end)) {
		/*
		 * THE HIDDEN WINDOWS, AS A LIST. `+3` used to STEP the row by
		 * one on every click — so reaching the third hidden window
		 * meant clicking three times and watching the whole bar
		 * reflow, and there was no way to see what was in there at
		 * all. kdos-teams is the window list this desktop already has;
		 * anchored to the cell that was clicked, it is the overflow
		 * menu every taskbar of this shape opens. The wheel over the
		 * row still steps, which is the gesture that wanted stepping.
		 */
		char xs[16], ys[16];
		snprintf(xs, sizeof(xs), "%d", plusn_x * kwl_cell_w());
		snprintf(ys, sizeof(ys), "%d", kwl_px_h());
		const char *argv[] = { "kdos-teams", panel_at_flag(), xs, ys,
				       NULL };
		panel_spawn(argv);
		return;
	}
	for (int i = 0; i < nfavs; i++)
		if (in_span(cx, fav_x[i], fav_end[i])) {
			/* Armed, not launched: the release decides, because
			 * this icon can also be dragged. See drag_fav. */
			drag_fav = i;
			drag_over = i;
			drag_moved = 0;
			return;
		}
	/* A half-open span like every other hit test here, and this is the one
	 * that has to be: the glyph is one cell at the very edge, and an
	 * open-ended comparison made the blank column beside it minimise the
	 * session. */
	if (in_span(cx, sh->show_hit_x, sh->show_hit_end)) {
		/*
		 * Show desktop = minimise everything. There is no "restore
		 * them all" here, and that is honest rather than lazy:
		 * kdos-comp has no iconified state of its own, so the panel
		 * would have to remember what it hid, and a memory that goes
		 * stale the moment a window closes is worse than a button that
		 * does one thing.
		 */
		for (int i = 0; i < sh->ntasks; i++)
			sh_minimize_task(sh, i);
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

/* The meters block's clock, shared rather than a second copy of it. */
static int64_t ah_now_ms(void)
{
	return panel_now_ms();
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
	int edge = KWL_EDGE_BOTTOM;
	int dump = 0, dump_w = 100;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else if (!strcmp(argv[i], "--dump-width") && i + 1 < argc)
			dump_w = atoi(argv[++i]);
		/* The bar is on the bottom edge now and `--bottom` is what
		 * every shipped configuration passed for a release, so it stays
		 * as a no-op rather than becoming a usage error at the next
		 * login on somebody's edited comp.conf. `--top` is the whole
		 * of what `panel = top` does. */
		else if (!strcmp(argv[i], "--bottom"))
			edge = KWL_EDGE_BOTTOM;
		else if (!strcmp(argv[i], "--top")) {
			edge = KWL_EDGE_TOP;
			/* Which way this bar's own popups have to grow — see
			 * panel_at_flag(). */
			panel_top = 1;
		}
		/* comp.conf's `panel_cells`. Two rows is the default because
		 * that is what makes a task button's icon square on a 16x32
		 * cell; one row is the pre-icon bar and still works. */
		else if (!strcmp(argv[i], "--cells") && i + 1 < argc) {
			tb_rows = atoi(argv[++i]);
			if (tb_rows < 1)
				tb_rows = 1;
			if (tb_rows > 4)
				tb_rows = 4;
		}
		/* comp.conf's `panel_autohide`, passed through by kdos-child. */
		else if (!strcmp(argv[i], "--autohide"))
			autohide = 1;
		else if (!strcmp(argv[i], "--no-icons"))
			icons_on = 0;
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
			printf("kdos-shell 0.2.0\n");
			return 0;
		} else {
			fprintf(stderr,
				"usage: kdos-shell [--top|--bottom] [--cells N] "
				"[--autohide] [--no-icons]\n"
				"                  [--output NAME] [--font NAME] "
				"[--clock FMT]\n"
				"       kdos-shell --dump [--dump-width N]\n");
			return 2;
		}
	}

	struct sh_state sh = {0};
	/* -1, not 0: a zeroed struct would light a chip for the whole session,
	 * which reads as a button that is stuck pressed. */
	sh.menu_open = -1;
	sh.hover_menu = -1;
	sh.hover_task = -1;
	KwlConfig cfg = {
		.role = KWL_ROLE_PANEL,
		.edge = edge,
		.cells = tb_rows,
		/* Must equal the .desktop id or the shell shows a second, unnamed
		 * icon for itself — the bug `kdos appid` exists to catch, and the
		 * one program with no excuse for it. */
		.app_id = "kdos-shell",
		.font = font,
		.output = output,
		.exclusive = 1,
		/*
		 * THE BAR IS FRAMED, like everything else on this desktop.
		 *
		 * Every KDOS surface but this one puts a double-line box round
		 * itself; the taskbar had no edge at all, so against a dark
		 * wallpaper it read as a region of the desktop rather than as
		 * a piece of chrome. A box wants four sides and two rows, and
		 * two rows is the whole bar — but the only edge a bottom-
		 * anchored panel HAS is its top one, and libkwl will draw that
		 * outside the cell grid for three pixels rather than a row.
		 */
		/* Five pixels: two of accent, a gap, two of accent — the
		 * cross-section of `═`, which is the mark the compositor draws
		 * every other window's frame with. See kwl_present(). */
		.rule = 5,
		.rule_slot = KT_ACCENT,
	};

	sh_theme_from_cache();

	/*
	 * `--dump` renders one frame offscreen and prints the cell grid.
	 *
	 * It is the same draw_taskbar() the surface uses — not a second
	 * description of what the panel contains, which would be a second
	 * thing to keep true. It reads REAL protocol state, so it needs the
	 * connection; it just does not need a surface.
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
		load_favorites();
		load_widgets();
		sh.mpris = sh_mpris_init(sh_tray_bus(&sh));
		sh_mpris_dispatch(sh.mpris);
		ktui_offscreen_init(dump_w, tb_rows);
		draw_taskbar(&sh);
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
	/*
	 * The icon layer, AFTER kwl_init because it needs the cell size and the
	 * output scale, and BEFORE the favorites because those resolve an icon
	 * name each. Failing is a desktop with no pictures, which is the one it
	 * had last week — every draw path here falls back to its glyph tier.
	 */
	if (icons_on)
		kicon_init(kwl_cell_w(), kwl_cell_h(), kwl_scale());
	/* Canvas text is the chrome's own family at whatever pixel size a tile
	 * asks for — the size in `--font` is the CELL's and is replaced per
	 * request. Off with the icons: a tile is a picture, and `icons = no`
	 * is a person asking for the character grid. */
	kcell_canvas_font(font);
	kch_tile_enable(icons_on);
	load_favorites();
	load_widgets();
	/* No session bus is a session with no tray, not a shell that refuses to
	 * start — a tty-launched panel for a screenshot has neither. */
	sh_tray_init(&sh);
	/* Same contract: no PipeWire is a session where nothing can be recording
	 * through it, not a shell that refuses to start. */
	sh_priv_init(&sh);
	/* AFTER the tray: it shares that connection rather than opening a
	 * second one, and sh_tray_bus() is NULL until the tray has one. */
	sh.mpris = sh_mpris_init(sh_tray_bus(&sh));
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
			/*
			 * AND panel.conf, which is the same signal doing the
			 * same job for a different file. kdos-settings writes
			 * `overflow`, `tray_hide`, `meters` and `task_labels`
			 * there and then SIGHUPs this panel; a setting that
			 * needed a logout to take effect would be a control
			 * panel that appears not to work. The compositor's own
			 * comp.conf keys are a different matter — several are
			 * a child's COMMAND LINE and say so.
			 */
			load_widgets();
			/* The pictures carry the accent too — they are tinted
			 * at load through kcol_remap — so a retint has to drop
			 * them, or the bar comes up in the new palette wearing
			 * the old icons. */
			kicon_retint();
			/* Every tile was rasterised in the palette that is
			 * being replaced — same reason, same moment. */
			kch_tile_reset();
			ktui_draw_invalidate();
		}
		/* Above the draw branch, not inside it: an autohidden panel
		 * draws the edge and nothing else, and the low-battery warning
		 * and the auto-suspend must not be a side effect of whether
		 * the pointer happens to be on the bar. */
		panel_tick(&sh);
		if (autohide && ah_hidden)
			ah_draw_edge();
		else
			draw_taskbar(&sh);

		KtuiEvent ev;
		/*
		 * A second, not a frame rate. The panel's fastest-changing
		 * thing is a clock that ticks once a minute; redrawing at
		 * display rate would burn a core to show the same pixels. Any
		 * event — a window appearing, a click — wakes it sooner.
		 */
		int wait = 1000;
		/*
		 * THE METERS' DEADLINE IS A CEILING ON THE WAIT.
		 *
		 * A chart draws one pixel per sample, so a sample that lands
		 * late draws a pixel late — and with a flat one-second wait
		 * against a half-second interval, any event at all (a pointer
		 * crossing the bar, a window mapping, a tray property) pushed
		 * the next sample most of a second past its due time. The
		 * graph stuck, then jumped. Same rule as the two below it and
		 * as libkwl's own poll for a key repeat: whoever has the
		 * nearest deadline sets the wait.
		 */
		int due = meters_due_in();
		if (due < wait)
			wait = due;
		/* The tooltip's dwell, by the same rule: whoever has the
		 * nearest deadline sets the wait. Without it a pointer that
		 * came to rest and produced no further event would get its tip
		 * up to a second late, or — on an idle bar with nothing else
		 * waking the loop — not at all. */
		int tdue = tip_due_in();
		if (tdue < wait)
			wait = tdue;
		/* A launch pulse needs a frame or two per beat, and NOTHING
		 * ELSE on this bar does: the shortened poll lasts exactly as
		 * long as the animation and an idle panel goes straight back
		 * to waking once a second. */
		if (fav_anim >= 0) {
			int64_t left = FAV_ANIM_MS -
				       (panel_now_ms() - fav_anim_at);
			if (left <= 0)
				fav_anim = -1;
			else if (wait > 60)
				wait = 60;
		}
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
				handle_motion(&sh, ev.mx, ev.my);
			} else if (ev.press == KT_MP_PRESS) {
				if (ev.btn == KT_MB_WHEEL_UP ||
				    ev.btn == KT_MB_WHEEL_DOWN) {
					handle_wheel(&sh, ev.mx,
						     ev.btn == KT_MB_WHEEL_UP);
				} else if (ev.btn == KT_MB_LEFT ||
					   ev.btn == KT_MB_MIDDLE ||
					   ev.btn == KT_MB_RIGHT) {
					handle_click(&sh, ev.mx, ev.my,
						     ev.btn == KT_MB_MIDDLE
							     ? SH_TRAY_BTN_MIDDLE
						     : ev.btn == KT_MB_RIGHT
							     ? SH_TRAY_BTN_RIGHT
							     : SH_TRAY_BTN_LEFT);
				}
			} else if (ev.press == KT_MP_RELEASE) {
				/* The quick-launch row is the only thing on
				 * this bar that cares: a press there ARMS a
				 * drag and the release either launches or
				 * reorders. Everything else acts on the press,
				 * which is what a panel button should do. */
				if (drag_fav >= 0)
					fav_release();
			}
		}
		/* The deadline is spent here rather than in the event branch:
		 * a leave with nothing following it produces no further event
		 * at all, and the hide has to happen anyway. */
		if (autohide && ah_hide_at && ah_now_ms() >= ah_hide_at)
			ah_hide();
		/* After the event, not before it: a motion that has just
		 * arrived has already restarted the dwell. */
		tip_tick(&sh);
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}
		sh_dispatch(&sh);
		sh_tray_dispatch(&sh);
		sh_priv_dispatch(&sh);
		sh_mpris_dispatch(sh.mpris);
	}

	sh_mpris_free(sh.mpris);
	sh_priv_free(&sh);
	sh_tray_free(&sh);
	sh_disconnect(&sh);
	kicon_finish();
	kwl_shutdown();
	return 0;
}
