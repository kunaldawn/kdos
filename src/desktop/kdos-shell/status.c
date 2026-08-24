/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-status — what is behind the chevron, and what it has to say
 *
 *   ╔══════════════════════════════════════════════════════════╗
 *   ║▓▓ Status                                               ▓▓║
 *   ║▓▓ 3 hidden — 1 wants attention                         ▓▓║
 *   ╟──────────────────────────────────────────────────────────╢
 *   ║ ▸ Stutter      14 frames dropped in the last 10 seconds  ║
 *   ║   Clipboard    12 entries kept                           ║
 *   ║   fcitx5       its menu is dbusmenu — not drawn here      ║
 *   ╟──────────────────────────────────────────────────────────╢
 *   ║ Enter opens             [ Panel… ] [ Close ]             ║
 *   ╚══════════════════════════════════════════════════════════╝
 *
 * TWO PROBLEMS, ONE WINDOW.
 *
 * The first is that half the notification area is OCCASIONAL — the stutter
 * chip, the restart mark, the clipboard depth — so the right wing changed
 * width whenever the machine had something to say and the whole bar appeared
 * to jerk. panel.c's chevron fixes that by giving those widgets one cell of
 * fixed width to live behind; this is the window that cell opens.
 *
 * The second is that the two most KDOS-specific things on the machine —
 * `kdos stutter`, which is the only tool anywhere that can say WHO made a
 * frame late, and `kdos-energy`, which does the same for battery — were
 * reachable only as `foot -e kdos stutter`: a terminal that covers the desktop
 * and scrolls a fresh paragraph every dropped frame until it is closed.
 * Reported exactly that way. So this runs them INSIDE the popup, streaming
 * their output into a pane that can be scrolled and stopped.
 *
 * THE PANEL PUBLISHES THE LIST AND THIS DRAWS IT — kdos-notify's split, and
 * for the same reason. Re-deriving "3 restarts" here would be a second
 * implementation of the same reading in a second program, asking /proc again
 * at the moment somebody clicked; instead the panel writes what it already
 * knows to $XDG_RUNTIME_DIR/kdos-panel.overflow when it spawns this.
 * ---------------------------------
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* KDOS ships basu; a development host usually has libsystemd, whose sd-bus is
 * the same API. The same selection tray.c and net.c make. */
#if defined(__has_include)
#  if __has_include(<basu/sd-bus.h>)
#    include <basu/sd-bus.h>
#  else
#    include <systemd/sd-bus.h>
#  endif
#else
#  include <basu/sd-bus.h>
#endif

#include "kicon.h"
#include "kwl.h"
#include "shell.h"

#define ST_COLS 76
#define ST_ROWS 24
#define LIST_COLS 56		/* the popup showing the list  */
#define LIST_ROWS 14
#define PANE_COLS 76		/* the popup showing a report  */
#define PANE_ROWS 22
#define ST_MAX 24		/* panel.c's OV_MAX */
#define ST_LINES 400		/* how much of a live report is kept */
#define ST_LINE 200

struct srow {
	char key[24];
	char icon[48];
	char label[48];
	char detail[96];
	int warn;
	char service[128];
	char path[128];
};

static struct srow rows[ST_MAX];
static int nrows;
static int sel, top, sel_follow = 1;
static int icons_on = 1;
static int popup;
static int list_y0 = 4;
static char status[128];

/* ── the live pane ─────────────────────────────────────────────────────── */

/*
 * A TOOL, RUN INSIDE THE WINDOW.
 *
 * The child's stdout is a pipe this loop reads without ever blocking on it:
 * libktui's poll has a timeout, so the loop wakes on its own every fifth of a
 * second while a pane is live and drains whatever has arrived. That is the
 * whole trick, and it is what makes `kdos stutter` — a program that prints
 * three lines an hour and then twenty in one second — readable in a popup
 * rather than in a terminal that has to be closed to get rid of it.
 */
static int pane_on;
static pid_t pane_pid = -1;
static int pane_fd = -1;
static int pane_done;
static char pane_title[64];
static char pane_cmd[64];
static char lines[ST_LINES][ST_LINE];
static int nlines, ltop, lfollow = 1;
static char pending[ST_LINE];
static size_t npending;

static void line_push(const char *s)
{
	/*
	 * SOFT-WRAPPED ON THE WAY IN. A stutter report is a hundred columns
	 * wide and this window is seventy-odd; clipping would throw away the
	 * end of every sentence, which on these two tools is the half that
	 * names the process. The surface never resizes while a pane is up, so
	 * wrapping once here is the same answer as wrapping at every draw.
	 */
	int w = ktui_w - 4;
	char rest[ST_LINE];
	int first = 1;

	if (w < 8)
		w = 8;
	snprintf(rest, sizeof(rest), "%s", s);
	for (;;) {
		char cut[ST_LINE], next[ST_LINE];

		sh_utf8_trunc(cut, sizeof(cut), rest, first ? w : w - 2);
		if (nlines >= ST_LINES) {
			/* By ROW: `lines + 1` points at a whole row inside the
			 * array, so the extent checked is the array. `lines[1]`
			 * is a pointer to one row and a fortified libc rejects
			 * a length that reaches past it. */
			memmove(lines, lines + 1,
				sizeof(lines[0]) * (ST_LINES - 1));
			nlines = ST_LINES - 1;
		}
		/* A wrapped continuation is indented, so a paragraph still
		 * reads as one thing rather than as two reports. The explicit
		 * precision is the gate's: two buffers of the same size and a
		 * two-space prefix is a truncation as far as the compiler is
		 * concerned, and it is right — the cut is already narrower. */
		if (first)
			snprintf(lines[nlines++], ST_LINE, "%s", cut);
		else
			snprintf(lines[nlines++], ST_LINE, "  %.*s",
				 ST_LINE - 4, cut);
		size_t used = strlen(cut);
		if (!used || !rest[used])
			break;
		const char *p = rest + used;
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		snprintf(next, sizeof(next), "%s", p);
		snprintf(rest, sizeof(rest), "%s", next);
		first = 0;
	}
}

static void pane_stop(void)
{
	if (pane_pid > 0) {
		kill(pane_pid, SIGTERM);
		waitpid(pane_pid, NULL, 0);
		pane_pid = -1;
	}
	if (pane_fd >= 0) {
		close(pane_fd);
		pane_fd = -1;
	}
	pane_done = 1;
}

static void pane_close(void)
{
	int was = pane_on;

	pane_stop();
	pane_on = 0;
	pane_done = 0;
	nlines = ltop = 0;
	npending = 0;
	lfollow = 1;
	if (was && popup)
		kwl_overlay_resize(LIST_COLS, LIST_ROWS);
}

/*
 * Fork the tool with its stdout on a pipe. No shell anywhere: every argument
 * here is a literal from the table below, and a command string would be one
 * more place for a label out of a .desktop file to become an argument.
 */
static int pane_open(const char *title, const char *const argv[])
{
	int fds[2];

	pane_close();
	if (pipe(fds) != 0)
		return -1;
	pid_t pid = fork();
	if (pid < 0) {
		close(fds[0]);
		close(fds[1]);
		return -1;
	}
	if (pid == 0) {
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO);
		dup2(fds[1], STDERR_FILENO);
		if (fds[1] > STDERR_FILENO)
			close(fds[1]);
		/* Its own session, so the SIGTERM that stops it cannot reach
		 * anything else and a Ctrl-C in whatever started this popup
		 * cannot reach it. */
		setsid();
		execvp(argv[0], (char *const *)argv);
		_exit(127);
	}
	close(fds[1]);
	fcntl(fds[0], F_SETFL, O_NONBLOCK);
	pane_fd = fds[0];
	pane_pid = pid;
	pane_on = 1;
	pane_done = 0;
	/* Room for a report — see the KwlConfig below. The first line or two
	 * are wrapped at the narrow width and stay that way, which costs
	 * nothing: `kdos stutter` opens with a 44-column sentence. */
	if (popup)
		kwl_overlay_resize(PANE_COLS, PANE_ROWS);
	snprintf(pane_title, sizeof(pane_title), "%s", title);
	snprintf(pane_cmd, sizeof(pane_cmd), "%s", argv[0]);
	return 0;
}

/* Whatever has arrived, split into lines. Never blocks. */
static void pane_pump(void)
{
	char buf[4096];
	ssize_t n;

	if (pane_fd < 0)
		return;
	while ((n = read(pane_fd, buf, sizeof(buf))) > 0) {
		for (ssize_t i = 0; i < n; i++) {
			if (buf[i] == '\n' || npending + 1 >= sizeof(pending)) {
				pending[npending] = '\0';
				line_push(pending);
				npending = 0;
				if (buf[i] != '\n')
					pending[npending++] = buf[i];
				continue;
			}
			if (buf[i] == '\r' || buf[i] == '\t')
				buf[i] = ' ';
			pending[npending++] = buf[i];
		}
	}
	if (n == 0) {
		/* EOF: the tool finished. Its last partial line still counts. */
		if (npending) {
			pending[npending] = '\0';
			line_push(pending);
			npending = 0;
		}
		close(pane_fd);
		pane_fd = -1;
		if (pane_pid > 0) {
			waitpid(pane_pid, NULL, 0);
			pane_pid = -1;
		}
		pane_done = 1;
	}
}

/* ── the table ─────────────────────────────────────────────────────────── */

static const char *table_path(char *out, size_t n, const char *given)
{
	const char *run = getenv("XDG_RUNTIME_DIR");

	if (given) {
		snprintf(out, n, "%s", given);
		return out;
	}
	if (!run || !*run)
		return NULL;
	snprintf(out, n, "%s/kdos-panel.overflow", run);
	return out;
}

static const char *field(const char *p, char *dst, size_t n)
{
	size_t i = 0;

	while (*p && *p != '\t' && *p != '\n') {
		if (i + 1 < n)
			dst[i++] = *p;
		p++;
	}
	dst[i] = '\0';
	return *p == '\t' ? p + 1 : p;
}

static void load_table(const char *given)
{
	char path[512], line[768];
	FILE *f;

	nrows = 0;
	if (!table_path(path, sizeof(path), given))
		return;
	f = fopen(path, "r");
	if (!f)
		return;
	while (nrows < ST_MAX && fgets(line, sizeof(line), f)) {
		struct srow *r = &rows[nrows];
		char warn[8];
		const char *p = line;

		memset(r, 0, sizeof(*r));
		p = field(p, r->key, sizeof(r->key));
		p = field(p, r->icon, sizeof(r->icon));
		p = field(p, r->label, sizeof(r->label));
		p = field(p, r->detail, sizeof(r->detail));
		p = field(p, warn, sizeof(warn));
		p = field(p, r->service, sizeof(r->service));
		field(p, r->path, sizeof(r->path));
		if (!r->key[0])
			continue;
		r->warn = atoi(warn);
		nrows++;
	}
	fclose(f);
	if (sel >= nrows)
		sel = nrows ? nrows - 1 : 0;
}

/* ── acting ────────────────────────────────────────────────────────────── */

/*
 * SNI Activate, straight from here.
 *
 * A hidden tray item is still an item, and the host that owns the protocol is
 * the panel — but a method call is a method call, and the address the panel
 * published in the table is all it takes. Fire and forget, like every other
 * call to a tray item: an app that has wedged must not wedge this.
 */
static void tray_activate(const struct srow *r)
{
	sd_bus *bus = NULL;

	if (!r->service[0] || !r->path[0])
		return;
	if (sd_bus_open_user(&bus) < 0 || !bus)
		return;
	if (sd_bus_call_method_async(bus, NULL, r->service, r->path,
				     "org.kde.StatusNotifierItem", "Activate",
				     NULL, NULL, "ii", 0, 0) < 0)
		sd_bus_call_method_async(bus, NULL, r->service, r->path,
					 "org.freedesktop.StatusNotifierItem",
					 "Activate", NULL, NULL, "ii", 0, 0);
	/* One flush, then go: the reply is of no interest and waiting for one
	 * is how a front end ends up hanging on somebody else's application. */
	sd_bus_flush(bus);
	sd_bus_unref(bus);
}

/*
 * What a key does. The three that stream are the point of this window; the
 * rest hand off to the program that owns them and this closes, because a popup
 * that stayed up behind the window it just opened is a popup in the way.
 *
 * Returns 1 when the caller should close.
 */
static int act_key(const char *key, const struct srow *r)
{
	if (!strcmp(key, "stutter")) {
		const char *argv[] = { "kdos", "stutter", NULL };

		if (pane_open("Stutter - who made the frame late", argv) != 0)
			snprintf(status, sizeof(status), "%s",
				 "could not run `kdos stutter`");
		return 0;
	}
	if (!strcmp(key, "restarts")) {
		const char *argv[] = { "kdos", "restarts", NULL };

		pane_open("Restarts - programs using replaced files", argv);
		return 0;
	}
	if (!strcmp(key, "energy")) {
		const char *argv[] = { "kdos-energy", NULL };

		pane_open("Energy - where the power went", argv);
		return 0;
	}
	if (!strcmp(key, "cpu")) {
		const char *argv[] = { "foot", "-e", "btop", NULL };

		sh_spawn(argv);
		return 1;
	}
	if (!strcmp(key, "clip")) {
		const char *argv[] = { "kdos-clip", "--pick", NULL };

		sh_spawn(argv);
		return 1;
	}
	if (!strcmp(key, "media") || !strcmp(key, "camera")) {
		const char *argv[] = { "kdos-devices", NULL };

		sh_spawn(argv);
		return 1;
	}
	if (!strcmp(key, "mic")) {
		const char *argv[] = { "kdos-audio", NULL };

		sh_spawn(argv);
		return 1;
	}
	if (!strcmp(key, "notify")) {
		const char *argv[] = { "kdos-notify", NULL };

		sh_spawn(argv);
		return 1;
	}
	if (!strcmp(key, "tray")) {
		if (r)
			tray_activate(r);
		snprintf(status, sizeof(status), "%s", "activated");
		return 0;
	}
	if (!strcmp(key, "mpris")) {
		snprintf(status, sizeof(status), "%s",
			 "the player's own controls are on the bar");
		return 0;
	}
	snprintf(status, sizeof(status), "nothing is wired to `%s`", key);
	return 0;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

enum { SB_OPEN = 0, SB_PANEL, SB_CLOSE, SB_N };
enum { PB_STOP = 0, PB_BACK, PB_CLOSE, PB_N };

static void draw_list(int w, int h)
{
	char sub[128];
	int warn = 0;

	for (int i = 0; i < nrows; i++)
		warn += rows[i].warn ? 1 : 0;
	if (!nrows)
		snprintf(sub, sizeof(sub), "%s", "nothing is hidden right now");
	else if (warn)
		snprintf(sub, sizeof(sub), "%d hidden - %d want%s attention",
			 nrows, warn, warn == 1 ? "s" : "");
	else
		snprintf(sub, sizeof(sub), "%d hidden", nrows);

	int y0 = kch_header(w, warn ? "dialog-warning" : "view-more",
				  "Status", sub, icons_on);
	list_y0 = y0;

	int vis = h - y0 - 2;

	if (vis < 1)
		vis = 1;
	int follow = sel_follow;

	sel_follow = 0;
	kch_list_clamp(&top, sel, nrows, vis, follow);

	int rw = nrows > vis ? w - 3 : w - 2;

	for (int i = 0; i < vis && top + i < nrows; i++) {
		const struct srow *r = &rows[top + i];
		int y = y0 + i;
		int on = top + i == sel;
		int fg = on ? KT_SURFACE : KT_TEXT;
		int bg = on ? KT_ACCENT : KT_BG;
		int ix = 2;

		ktui_draw_fill(krect(1, y, rw, 1), bg);
		if (icons_on) {
			int slot = kicon_slot(r->icon, 2, 1);

			if (slot >= 0) {
				ktui_draw_sprite(krect(2, y, 2, 1), slot,
						 r->warn && !on ? KT_WARN : fg,
						 bg);
				ix = 5;
			}
		}
		ktui_draw_text(ix, y, 14, r->label,
			       on ? fg : (r->warn ? KT_WARN : KT_TEXT), bg,
			       KT_A_NONE);
		if (ix + 15 < rw)
			ktui_draw_text(ix + 15, y, rw - ix - 15, r->detail,
				       on ? fg : KT_MID, bg, KT_A_NONE);
	}
	if (!nrows)
		ktui_draw_text(2, y0, w - 4,
			       "Widgets named by `overflow =` in panel.conf "
			       "live here.",
			       KT_MID, KT_BG, KT_A_NONE);
	kch_scrollbar(0, w - 2, y0, vis, nrows, top, KT_BG);

	struct kch_button b[SB_N];

	b[SB_OPEN] = (struct kch_button){ "Open", nrows > 0 };
	/* No ellipsis: the ascii tier draws every non-ASCII codepoint as `?`,
	 * so `Panel…` reads as `Panel?` on tty1 and in a golden frame. */
	b[SB_PANEL] = (struct kch_button){ "Panel", 1 };
	b[SB_CLOSE] = (struct kch_button){ "Close", 1 };
	int bx = kch_buttons(w, h - 2, b, SB_N, -1);
	int room = bx - 3;
	static const char HINT[] = "Enter opens   Esc closes";

	if (room > 0 &&
	    (status[0] ? room >= 8 : room >= (int)ktui_utf8_width(HINT)))
		ktui_draw_text(2, h - 2, room, status[0] ? status : HINT,
			       status[0] ? KT_WARN : KT_MID, KT_BG, KT_A_NONE);
}

static void draw_pane(int w, int h)
{
	char sub[128];

	snprintf(sub, sizeof(sub), "%s%s", pane_cmd,
		 pane_done ? " - finished" : " - running");
	/* `speedometer`, checked against the SHIPPED ATLAS —
	 * `utilities-system-monitor` is the obvious name and Papirus files it
	 * in a context vendor.py does not take, so the band would have come up
	 * with a hole in it. */
	int y0 = kch_header(w, "speedometer", pane_title, sub, icons_on);

	/* Recorded from what was DRAWN, like every list in this shell: the
	 * scroll arithmetic below reads it, and deriving that origin twice is
	 * how a viewport ends up one row out. */
	list_y0 = y0;

	int vis = h - y0 - 2;

	if (vis < 1)
		vis = 1;
	/*
	 * IT FOLLOWS THE TAIL UNTIL SOMEBODY SCROLLS. A live report that
	 * jumped back to the newest line under a hand that had just scrolled
	 * up would be unreadable — the same rule the lists keep with
	 * sel_follow, arrived at from the other side.
	 */
	if (lfollow)
		ltop = nlines > vis ? nlines - vis : 0;
	if (ltop > nlines - vis)
		ltop = nlines > vis ? nlines - vis : 0;
	if (ltop < 0)
		ltop = 0;

	for (int i = 0; i < vis; i++) {
		int idx = ltop + i;

		ktui_draw_fill(krect(1, y0 + i, w - 2, 1), KT_BG);
		if (idx < nlines)
			ktui_draw_text(2, y0 + i, w - 4, lines[idx], KT_TEXT,
				       KT_BG, KT_A_NONE);
	}
	if (!nlines)
		ktui_draw_text(2, y0, w - 4,
			       pane_done ? "it printed nothing"
					 : "waiting for it to say something…",
			       KT_MID, KT_BG, KT_A_NONE);
	kch_scrollbar(1, w - 2, y0, vis, nlines, ltop, KT_BG);

	struct kch_button b[PB_N];

	b[PB_STOP] = (struct kch_button){ "Stop", !pane_done };
	b[PB_BACK] = (struct kch_button){ "Back", 1 };
	b[PB_CLOSE] = (struct kch_button){ "Close", 1 };
	int bx = kch_buttons(w, h - 2, b, PB_N, -1);
	int room = bx - 3;
	static const char HINT[] = "wheel scrolls   End follows   Esc back";

	if (room >= (int)ktui_utf8_width(HINT))
		ktui_draw_text(2, h - 2, room, HINT, KT_MID, KT_BG, KT_A_NONE);
}

static void draw_frame(void)
{
	int w = ktui_w, h = ktui_h;

	if (w < 24 || h < 8)
		return;
	ktui_draw_fill(krect(0, 0, w, h), KT_BG);
	sh_frame(w, h, NULL, KT_ACCENT, KT_BG, 1);
	if (pane_on)
		draw_pane(w, h);
	else
		draw_list(w, h);
	ktui_draw_flush();
}

/* ── main ──────────────────────────────────────────────────────────────── */

int status_main(int argc, char **argv)
{
	const char *font = NULL, *from = NULL, *open = NULL;
	int at_x = -1, at_y = 0, at_bottom = 0, dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--at") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--at-bottom") && i + 2 < argc) {
			at_x = atoi(argv[++i]);
			at_y = atoi(argv[++i]);
			at_bottom = 1;
		} else if (!strcmp(argv[i], "--open") && i + 1 < argc) {
			open = argv[++i];
		} else if (!strcmp(argv[i], "--from") && i + 1 < argc) {
			from = argv[++i];
		} else if (!strcmp(argv[i], "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(argv[i], "--no-icons")) {
			icons_on = 0;
		} else if (!strcmp(argv[i], "--dump")) {
			dump = 1;
		} else {
			fprintf(stderr, "usage: kdos-status [--at X Y] "
					"[--at-bottom X Y] [--open KEY] "
					"[--from FILE] [--font NAME] "
					"[--no-icons] [--dump]\n");
			return 2;
		}
	}

	popup = at_x >= 0;
	load_table(from);

	if (dump) {
		sh_theme_from_cache();
		icons_on = 0;
		ktui_offscreen_init(ST_COLS, ST_ROWS);
		draw_frame();
		ktui_draw_dump();
		return 0;
	}

	/*
	 * A POPUP IS THE SIZE OF WHAT IT SHOWS, and this shows two different
	 * things. The LIST is three or four rows and a bar of that width across
	 * half the screen reads as a window that has lost its way; a REPORT is a
	 * hundred columns of somebody else's output and wants every column it
	 * can get. So the popup opens small and grows when a pane opens — which
	 * a layer surface can do, and the deep link (`--open`, what the stutter
	 * chip and the meters strip use) opens at the report's size directly.
	 */
	KwlConfig cfg = {
		/*
		 * ANCHORED MEANS POPUP; CENTRED MEANS A WINDOW — and a window
		 * is an xdg TOPLEVEL, not a layer surface. Layer-shell has no
		 * move and no resize in the protocol at all, so every native
		 * app on this desktop was a rectangle nailed to the screen
		 * while every boxed one could be dragged and pulled about. A
		 * toplevel also gets the compositor's own frame, which is the
		 * other half of it: the decoration then MATCHES an alien app's
		 * because it IS an alien app's.
		 */
		.role = popup ? KWL_ROLE_OVERLAY : KWL_ROLE_TOPLEVEL,
		.cols = popup ? (open ? PANE_COLS : LIST_COLS) : ST_COLS,
		.rows = popup ? (open ? PANE_ROWS : LIST_ROWS) : ST_ROWS,
		.corner = !popup	? KWL_CORNER_CENTER
			  : at_bottom	? KWL_CORNER_BOTTOM_LEFT
					: KWL_CORNER_TOP_LEFT,
		.margin_x = popup ? at_x : 0,
		.margin_y = popup ? at_y : 0,
		/* The SSD shows this: a toplevel with no title gets an
		 * empty titlebar, which is a frame that says nothing. */
		.title = "Status",
		.app_id = "kdos-status",
		.font = font,
		.keyboard = 1,
		.dismiss_on_unfocus = popup,
	};

	sh_theme_from_cache();
	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-status: no compositor or no layer-shell\n");
		return 1;
	}
	if (icons_on)
		kicon_init(kwl_cell_w(), kwl_cell_h(), kwl_scale());
	ktui_draw_init();
	/* The bar's own body, so a popup over the taskbar is the
	 * same surface the taskbar is — see kch_px_popup(). */
	kch_px_popup(KT_BG);

	/* `--open KEY` is a deep link and needs no table row: the panel uses it
	 * for the stutter chip and the meters strip, which are cells on the bar
	 * rather than entries in the overflow. */
	if (open && act_key(open, NULL))
		goto done;

	while (!kwl_should_close()) {
		sh_theme_poll();
		pane_pump();
		draw_frame();

		KtuiEvent ev;
		/* A live pane is the only thing here that changes on its own,
		 * and a fifth of a second is a report that scrolls rather than
		 * one that arrives in lumps. */
		if (!ktui_backend()->poll_event(&ev, pane_on ? 200 : 1000)) {
			if (ktui_resized) {
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			if (!pane_on)
				load_table(from);
			continue;
		}

		if (ev.type == KT_EVT_MOUSE) {
			int y0 = list_y0;
			int vis = ktui_h - y0 - 2;
			int idx = top + ev.my - y0;
			int in_list = !pane_on && ev.my >= y0 &&
				      ev.my < y0 + vis && idx >= 0 &&
				      idx < nrows;

			if (ev.press == KT_MP_DRAG) {
				/* THE BAR IS A CONTROL — see kch_scrollbar. */
				int bt = kch_scrollbar_drag(ev.my);

				if (bt >= 0) {
					if (kch_scrollbar_grabbed() == 0)
						top = bt;
					else
						ltop = bt;
					sel_follow = 0;
					continue;
				}
				if (in_list) {
					sel = idx;
					sel_follow = 1;
				}
				kch_hover(ev.mx, ev.my);
				continue;
			}
			if (ev.press == KT_MP_RELEASE) {
				kch_scrollbar_release();
				continue;
			}
			if (ev.press != KT_MP_PRESS)
				continue;
			if (ev.btn == KT_MB_LEFT) {
				int bt;

				bt = kch_scrollbar_press(0, ev.mx, ev.my);
				if (bt >= 0) {
					top = bt;
					sel_follow = 0;
					continue;
				}
				bt = kch_scrollbar_press(1, ev.mx, ev.my);
				if (bt >= 0) {
					ltop = bt;
					sel_follow = 0;
					continue;
				}
			}
			if (ev.btn == KT_MB_WHEEL_UP ||
			    ev.btn == KT_MB_WHEEL_DOWN) {
				int up = ev.btn == KT_MB_WHEEL_UP;

				if (pane_on) {
					int pv = ktui_h - list_y0 - 2;

					lfollow = 0;
					ltop += up ? -SH_WHEEL_ROWS
						   : SH_WHEEL_ROWS;
					if (ltop < 0)
						ltop = 0;
					if (ltop >= nlines - pv)
						lfollow = 1;
					continue;
				}
				if (!kch_list_wheel(up, &top, nrows, vis)) {
					sel += up ? -1 : 1;
					if (sel < 0)
						sel = 0;
					if (sel >= nrows)
						sel = nrows ? nrows - 1 : 0;
					sel_follow = 1;
				}
				continue;
			}
			if (ev.btn == KT_MB_RIGHT) {
				if (pane_on) {
					pane_close();
					continue;
				}
				break;
			}
			if (ev.btn != KT_MB_LEFT)
				continue;

			int bi = kch_button_at(ev.mx, ev.my);

			if (pane_on) {
				if (bi == PB_STOP)
					pane_stop();
				else if (bi == PB_BACK)
					pane_close();
				else if (bi == PB_CLOSE)
					break;
				continue;
			}
			if (bi == SB_CLOSE)
				break;
			if (bi == SB_PANEL) {
				const char *a[] = { "kdos-settings", "--page",
						    "panel", NULL };
				sh_spawn(a);
				break;
			}
			if (bi == SB_OPEN && sel < nrows) {
				if (act_key(rows[sel].key, &rows[sel]))
					break;
				continue;
			}
			if (in_list) {
				if (idx == sel) {
					if (act_key(rows[sel].key, &rows[sel]))
						break;
				}
				sel = idx;
				sel_follow = 1;
			}
			continue;
		}

		if (ev.type != KT_EVT_KEY)
			continue;
		if (pane_on) {
			int pv = ktui_h - list_y0 - 2;

			switch (ev.key) {
			case KT_K_ESC:
				pane_close();
				break;
			case KT_K_UP:
				lfollow = 0;
				if (ltop > 0)
					ltop--;
				break;
			case KT_K_DOWN:
				ltop++;
				if (ltop >= nlines - pv)
					lfollow = 1;
				break;
			case KT_K_HOME:
				lfollow = 0;
				ltop = 0;
				break;
			case KT_K_END:
				lfollow = 1;
				break;
			case 's':
				pane_stop();
				break;
			default:
				break;
			}
			continue;
		}
		sel_follow = 1;
		switch (ev.key) {
		case KT_K_ESC:
			goto done;
		case KT_K_UP:
			if (sel > 0)
				sel--;
			break;
		case KT_K_DOWN:
			if (sel + 1 < nrows)
				sel++;
			break;
		case KT_K_HOME:
			sel = 0;
			break;
		case KT_K_END:
			sel = nrows ? nrows - 1 : 0;
			break;
		case KT_K_ENTER:
			if (sel < nrows && act_key(rows[sel].key, &rows[sel]))
				goto done;
			break;
		case 'r':
			load_table(from);
			break;
		default:
			break;
		}
	}
done:
	pane_close();
	kicon_finish();
	kwl_shutdown();
	return 0;
}
