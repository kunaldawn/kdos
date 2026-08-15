/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-slit — the dockapp column, reinvented for cells
 *
 *   ┌──────────┐
 *   │ 41%  3.2 │
 *   │ up 4d 2h │
 *   │ !        │
 *   └──────────┘
 *
 * WindowMaker's dock was the best idea in that generation of desktops and the
 * worst implementation: every gadget was an X11 client that had to be WRITTEN as
 * a dockapp, which is why there are two hundred of them and all of them are from
 * 2003. On a character grid the gadget is a LINE OF TEXT, so anything that can
 * print one is a dockapp — `date`, a script, `cat /proc/loadavg`.
 *
 * THE CONFIG LINE IS ARGV. `<interval_s> <width_cells> <command> [args...]`,
 * split on whitespace and exec'd directly, exactly like the alien-apps table:
 * there is no shell anywhere in this program, so nothing a gadget prints and
 * nothing in its name can become an injection point. A user who wants a pipe
 * writes a script and names the script — which is one file more and one whole
 * class of surprise less.
 *
 * A gadget that FAILS shows a dim `!` and keeps its row. Vanishing would be the
 * worse answer twice over: the column would reflow under the user's eyes, and
 * the one state worth seeing — "the thing I am monitoring stopped answering" —
 * would be indistinguishable from "I never configured it".
 *
 * NOTHING STARTS IT YET. There is no `slit` key in comp.conf and no row in
 * kdos-child.c's TEMPLATES[], so writing ~/.config/kdos/slit.conf does not make
 * a column appear: this is run by hand or from a keybind until the compositor
 * carries a supervised child for it, one per output, the way kdos-desk is.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define SL_MAX_GADGETS	32
#define SL_MAX_ARGS	16
#define SL_LINE		512
#define SL_OUT		256
#define SL_MIN_WIDTH	1
#define SL_MAX_WIDTH	64

struct sl_gadget {
	char line[SL_LINE];		/* the tokens live here: kb_argv_add
					 * stores the POINTER and does not copy,
					 * so the storage has to outlive the run */
	const char *argv[SL_MAX_ARGS + 1];
	int nargv;
	int interval;			/* seconds */
	int width;			/* cells   */
	time_t next;			/* when it is due  */
	char out[SL_OUT];		/* its first line  */
	int failed;			/* last run said no */
	int reported;			/* "no such command" is said ONCE */
};

static struct sl_gadget sl_g[SL_MAX_GADGETS];
static int sl_n;
static int sl_width = 1;

/* ── the config ────────────────────────────────────────────────────────── */

static int sl_conf_path(char *buf, size_t n)
{
	const char *cfg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (cfg && *cfg)
		snprintf(buf, n, "%s/kdos/slit.conf", cfg);
	else if (home && *home)
		snprintf(buf, n, "%s/.config/kdos/slit.conf", home);
	else
		return -1;
	return 0;
}

/* Split in place. The line buffer belongs to the gadget and stays alive for the
 * process, which is what lets the argv point straight into it. */
static void sl_split(struct sl_gadget *g, char *rest)
{
	char *save = NULL;

	g->nargv = 0;
	for (char *t = strtok_r(rest, " \t", &save);
	     t && g->nargv < SL_MAX_ARGS; t = strtok_r(NULL, " \t", &save))
		g->argv[g->nargv++] = t;
	g->argv[g->nargv] = NULL;
}

/*
 * Returns the number of gadgets, or -1 when there is no config file at all —
 * which is not an error. A slit with nothing in it is a strip of reserved
 * screen showing nothing, so the answer to "no config" is to exit rather than
 * to draw an empty column.
 */
static int sl_load(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[SL_LINE];

	if (!f)
		return -1;
	while (sl_n < SL_MAX_GADGETS && fgets(line, sizeof(line), f)) {
		char *p = line;

		line[strcspn(line, "\r\n")] = '\0';
		while (*p == ' ' || *p == '\t')
			p++;
		if (!*p || *p == '#')
			continue;

		struct sl_gadget *g = &sl_g[sl_n];
		memset(g, 0, sizeof(*g));
		snprintf(g->line, sizeof(g->line), "%s", p);

		char *save = NULL;
		char *iv = strtok_r(g->line, " \t", &save);
		char *wd = iv ? strtok_r(NULL, " \t", &save) : NULL;
		char *cmd = wd ? save : NULL;
		if (!cmd)
			continue;
		while (*cmd == ' ' || *cmd == '\t')
			cmd++;
		if (!*cmd)
			continue;

		g->interval = atoi(iv);
		if (g->interval < 1)
			g->interval = 1;
		g->width = atoi(wd);
		if (g->width < SL_MIN_WIDTH)
			g->width = SL_MIN_WIDTH;
		if (g->width > SL_MAX_WIDTH)
			g->width = SL_MAX_WIDTH;
		sl_split(g, cmd);
		if (!g->nargv)
			continue;
		if (g->width > sl_width)
			sl_width = g->width;
		sl_n++;
	}
	fclose(f);
	return sl_n;
}

/* ── running one ───────────────────────────────────────────────────────── */

/*
 * A gadget prints TEXT, and the grid takes text rather than commands. A program
 * that thinks it has a tty writes SGR (`ls --color=always`, figlet), and
 * ktui_wcwidth gives every byte under 0x300 a cell — so the escapes would be
 * drawn as `[1;32m` and the C0 range would move the cursor of whatever reads a
 * --dump. CSI sequences go entirely; anything else below 0x20 becomes a space.
 */
static void sl_clean(char *s)
{
	char *w = s;

	for (const char *r = s; *r; ) {
		if (*r == '\033') {
			r++;
			if (*r == '[') {
				r++;
				while (*r && (unsigned char)*r < 0x40)
					r++;
				if (*r)
					r++;
			} else if (*r) {
				r++;	/* a two-byte escape */
			}
			continue;
		}
		*w++ = (unsigned char)*r < 0x20 ? ' ' : *r;
		r++;
	}
	*w = '\0';
}

/*
 * Synchronous, and the one thing to know about it: kb_run_capture waits for the
 * child, so a gadget that never exits stops this surface redrawing until it
 * does. That is the gadget's bug rather than the slit's — the config line is a
 * command the user chose — and the alternative is a per-gadget child, a poll set
 * and a timeout, which is a supervisor rather than a dock. A gadget that might
 * hang belongs behind a script with its own timeout.
 */
static void sl_run(struct sl_gadget *g, time_t now)
{
	KbArgv a = {0};
	char buf[SL_OUT];
	int rc;

	for (int i = 0; i < g->nargv; i++)
		kb_argv_add(&a, g->argv[i]);
	kb_argv_end(&a);

	rc = kb_run_capture(&a, buf, sizeof(buf));
	g->next = now + g->interval;

	/* 127 is kb_proc's exec failure, not a program that ran and said no.
	 * Reported once, because a gadget naming a command that is not
	 * installed would otherwise write a line per interval for the whole
	 * session into whatever the session log is. */
	if (rc == 127) {
		if (!g->reported) {
			fprintf(stderr, "kdos-slit: %s: not found\n",
				g->argv[0]);
			g->reported = 1;
		}
		g->failed = 1;
		g->out[0] = '\0';
		return;
	}
	if (rc != 0) {
		g->failed = 1;
		g->out[0] = '\0';
		return;
	}

	/* The FIRST line. A gadget that prints a paragraph gets one row, and
	 * taking the head of it is more useful than taking the tail. */
	buf[strcspn(buf, "\r\n")] = '\0';
	sl_clean(buf);
	g->failed = 0;
	snprintf(g->out, sizeof(g->out), "%s", buf);
}

static void sl_tick(time_t now)
{
	for (int i = 0; i < sl_n; i++)
		if (now >= sl_g[i].next)
			sl_run(&sl_g[i], now);
}

/* How long until the next gadget is due, in ms, bounded so the loop still
 * answers a resize promptly. */
static int sl_wait_ms(time_t now)
{
	int best = 60;

	for (int i = 0; i < sl_n; i++) {
		int d = (int)(sl_g[i].next - now);
		if (d < 0)
			d = 0;
		if (d < best)
			best = d;
	}
	return best * 1000 + 50;
}

/* ── drawing ───────────────────────────────────────────────────────────── */

static void sl_draw(void)
{
	int w = ktui_w, h = ktui_h;

	if (w < 1 || h < 1)
		return;
	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);

	for (int i = 0; i < sl_n && i < h; i++) {
		const struct sl_gadget *g = &sl_g[i];
		int gw = g->width < w ? g->width : w;

		if (g->failed) {
			/* Dim, and its own glyph: a gadget that is broken must
			 * not look like a gadget that is reporting a value. */
			ktui_draw_text(0, i, gw, "!", KT_DIM, KT_SURFACE,
				       KT_A_NONE);
			continue;
		}
		ktui_draw_text(0, i, gw, g->out, KT_TEXT, KT_SURFACE,
			       KT_A_NONE);
	}
	ktui_draw_flush();
}

/* ── main ──────────────────────────────────────────────────────────────── */

static int sl_usage(void)
{
	fprintf(stderr,
		"usage: kdos-slit [--output NAME] [--font NAME] "
		"[--config PATH] [--dump]\n");
	return 2;
}

int slit_main(int argc, char **argv)
{
	const char *font = NULL, *output = NULL, *conf = NULL;
	char path[512];
	int dump = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--font") && i + 1 < argc)
			font = argv[++i];
		else if (!strcmp(argv[i], "--output") && i + 1 < argc)
			output = argv[++i];
		else if (!strcmp(argv[i], "--config") && i + 1 < argc)
			conf = argv[++i];
		else if (!strcmp(argv[i], "--dump"))
			dump = 1;
		else
			return sl_usage();
	}

	if (conf)
		snprintf(path, sizeof(path), "%s", conf);
	else if (sl_conf_path(path, sizeof(path)) != 0)
		return 0;

	/*
	 * No config is not a failure and says nothing: kdos-desktop-start may
	 * launch this unconditionally, and a machine that has not configured a
	 * slit must not get a warning at every login for a feature it is not
	 * using.
	 */
	if (sl_load(path) <= 0)
		return 0;

	sh_theme_from_cache();

	if (dump) {
		/* The gadgets are RUN for a dump — the alternative is a picture
		 * of an empty column, which says nothing about the layout the
		 * dump exists to show. */
		ktui_offscreen_init(sl_width, sl_n);
		sl_tick(time(NULL));
		sl_draw();
		ktui_draw_dump();
		return 0;
	}

	KwlConfig cfg = {
		/*
		 * A PANEL on the right edge, not an overlay: the exclusive zone
		 * is the whole point. A column of gadgets that windows can
		 * cover is a column nobody sees, and the zone is what a
		 * maximised window is kept out of.
		 */
		.role = KWL_ROLE_PANEL,
		.edge = KWL_EDGE_RIGHT,
		.cells = sl_width,
		.exclusive = 1,
		.app_id = "kdos-slit",
		.font = font,
		.output = output,
		/* Never the keyboard. This is a display, and a strip of text
		 * that stole focus from the window being typed into every time
		 * a gadget refreshed would be unusable. */
	};

	if (kwl_init(&cfg) != 0) {
		fprintf(stderr, "kdos-slit: no compositor or no layer-shell\n");
		return 1;
	}
	ktui_draw_init();
	sh_theme_watch();

	sl_tick(time(NULL));

	while (!kwl_should_close()) {
		if (sh_theme_dirty) {
			sh_theme_dirty = 0;
			sh_theme_from_cache();
			ktui_draw_invalidate();
		}
		sl_draw();

		KtuiEvent ev;
		if (ktui_backend()->poll_event(&ev, sl_wait_ms(time(NULL))) &&
		    ev.type == KT_EVT_MOUSE && ev.press == KT_MP_PRESS &&
		    ev.btn == KT_MB_LEFT && ev.my >= 0 && ev.my < sl_n) {
			/* A click refreshes that gadget now. The one thing a
			 * person wants from a value that updates every five
			 * minutes is to not wait five minutes for it. */
			sl_run(&sl_g[ev.my], time(NULL));
		}
		if (ktui_resized) {
			ktui_resized = 0;
			ktui_draw_resize();
			ktui_draw_invalidate();
		}
		sl_tick(time(NULL));
	}

	kwl_shutdown();
	return 0;
}
