/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The CLI, the backend choice, and the one loop.
 *
 * THREE FACES, ONE DRAW. Whether the cells end up in a terminal, in a window
 * or in a text dump is libktui's backend vtable's business; res_draw_frame()
 * is called by all three and knows about none of them. That is what makes the
 * golden frames worth anything: they are the same layout the window draws.
 *
 * THE SAMPLE DEADLINE IS ALSO THE POLL DEADLINE. This loop is woken by events
 * — a keystroke, a pointer crossing a row, a configure — so a poll that always
 * waits the full interval samples at irregular intervals, and a chart plots one
 * sample per pixel. The wait is always what remains until the next sample and
 * never more.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "kwl.h"
#include "res.h"

static void usage(FILE *f)
{
	fprintf(f,
	  "usage: kdos-res [--page <id>] [--tty|--gui]\n"
	  "                [--fixture <dir>] [--interval <ms>] [--detail <pid>]\n"
	  "                [--dump|--dump-cells] [--dump-size WxH] [--json]\n"
	  "                [--version] [--help]\n"
	  "\n"
	  "pages: applications processes cpu memory gpu drives network\n"
	  "       batteries energy\n");
}

/*
 * The accent, from the one word kdos theme writes. No colours are read: the
 * palette is compiled in through libkcolor, and the state file names which of
 * its schemes is in force.
 */
void res_theme_from_cache(void)
{
	char path[512], name[64];
	const char *cache = getenv("XDG_CACHE_HOME");
	const char *home = getenv("HOME");

	if (cache && *cache)
		snprintf(path, sizeof(path), "%s/kdos/theme", cache);
	else if (home && *home)
		snprintf(path, sizeof(path), "%s/.cache/kdos/theme", home);
	else
		return;

	char *d = kb_read_all(path, NULL);
	if (!d)
		return;		/* absent: ktui_theme_set already defaulted */
	char *nl = strchr(d, '\n');
	if (nl)
		*nl = 0;
	kb_strlcpy(name, d, sizeof(name));
	free(d);
	if (*name)
		ktui_theme_set(name);
}

/*
 * SIGHUP: re-read res.conf and the accent, on the same signal `kdos theme`
 * already sends. A flag rather than the work itself — a handler that reparsed
 * a file would be doing allocation inside a signal, and the loop is never more
 * than one interval away from noticing.
 */
static volatile sig_atomic_t g_reload;

static void on_hup(int sig)
{
	(void)sig;
	g_reload = 1;
}

int main(int argc, char **argv)
{
	const char *page = NULL, *fixture = NULL, *font = NULL;
	int detail_pid = 0;
	int want_tty = 0, want_gui = 0, dump = 0, dump_cells = 0, json = 0;
	int dw = 80, dh = 24, interval = 0;

	kb_set_progname("kdos-res");

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "--page") && i + 1 < argc) {
			page = argv[++i];
		} else if (!strcmp(a, "--detail") && i + 1 < argc) {
			/*
			 * Dump-only, and it exists so the detail page has a
			 * golden. Every other surface in this program can be
			 * reached by --page; this one is opened with Enter on
			 * a row, and a page with no golden is a page whose
			 * geometry nothing checks.
			 */
			detail_pid = atoi(argv[++i]);
		} else if (!strcmp(a, "--fixture") && i + 1 < argc) {
			fixture = argv[++i];
		} else if (!strcmp(a, "--interval") && i + 1 < argc) {
			interval = atoi(argv[++i]);
		} else if (!strcmp(a, "--font") && i + 1 < argc) {
			font = argv[++i];
		} else if (!strcmp(a, "--dump-size") && i + 1 < argc) {
			if (sscanf(argv[++i], "%dx%d", &dw, &dh) != 2) {
				fprintf(stderr, "kdos-res: --dump-size wants WxH\n");
				return 2;
			}
		} else if (!strcmp(a, "--dump")) {
			dump = 1;
		} else if (!strcmp(a, "--dump-cells")) {
			dump = dump_cells = 1;
		} else if (!strcmp(a, "--json")) {
			json = 1;
		} else if (!strcmp(a, "--tty")) {
			want_tty = 1;
		} else if (!strcmp(a, "--gui")) {
			want_gui = 1;
		} else if (!strcmp(a, "--version")) {
			printf("kdos-res " KDOS_RES_VERSION "\n");
			return 0;
		} else if (!strcmp(a, "--help") || !strcmp(a, "-h")) {
			usage(stdout);
			return 0;
		} else {
			/*
			 * An unknown flag EXITS 2 with a usage line. A front
			 * end that accepted anything would fail silently when
			 * another program spawned it with a flag it does not
			 * have, which is the class testing/preflight.sh's flag
			 * check exists to catch.
			 */
			fprintf(stderr, "kdos-res: unknown option '%s'\n", a);
			usage(stderr);
			return 2;
		}
	}

	if (fixture) {
		char p[512], s[512], pw[512];
		snprintf(p, sizeof(p), "%s/proc", fixture);
		snprintf(s, sizeof(s), "%s/sys", fixture);
		kpr_root_set(p, s);
		/*
		 * The user database is the fixture's too. Resolving a uid
		 * against the developer's own /etc/passwd makes a recorded
		 * machine render a different name on every host, and a golden
		 * frame cannot survive that.
		 */
		snprintf(pw, sizeof(pw), "%s/passwd", fixture);
		if (kb_path_exists(pw))
			setenv("KPR_PASSWD", pw, 1);
		R.fixture = 1;
	}

	res_conf_load();
	if (interval > 0)
		RC.interval_ms = interval < 200 ? 200 : interval;

	if (page) {
		int idx = res_page_index(page);
		if (idx < 0) {
			fprintf(stderr, "kdos-res: unknown page '%s'\n", page);
			usage(stderr);
			return 2;
		}
		res_page_set(idx);
	}

	res_theme_from_cache();

	/* ── the dump face ─────────────────────────────────────────────── */
	if (dump) {
		if (ktui_offscreen_init(dw, dh) != 0) {
			fprintf(stderr, "kdos-res: cannot render offscreen\n");
			return 1;
		}
		/*
		 * TWO samples before drawing, and against the fixture's TWO
		 * snapshots. Every rate on every page is a difference between
		 * two readings, so sampling one snapshot twice renders a
		 * machine doing nothing at all — and a golden of that locks in
		 * a picture that can never catch an arithmetic error.
		 *
		 * <fixture>/ is the first and <fixture>/next/ the second, one
		 * interval apart.
		 */
		res_sample();
		if (fixture) {
			char p2[512], s2[512];
			snprintf(p2, sizeof(p2), "%s/next/proc", fixture);
			snprintf(s2, sizeof(s2), "%s/next/sys", fixture);
			if (kb_path_exists(p2))
				kpr_root_set(p2, s2);
		}
		res_sample();
		if (detail_pid)
			res_detail_open_proc(detail_pid);
		res_draw_frame();
		if (json)
			return 0;
		ktui_draw_dump();
		(void)dump_cells;
		return 0;
	}

	/* ── the window, or the terminal ───────────────────────────────── */
	int gui = want_gui;
	if (!want_tty && !want_gui) {
		const char *wd = getenv("WAYLAND_DISPLAY");
		gui = wd && *wd;
	}

	if (gui) {
		KwlConfig cfg = {
			.role = KWL_ROLE_TOPLEVEL,
			.title = "Resources",
			.app_id = "kdos-res",
			.font = font,
			.keyboard = 1,
			/*
			 * At or above RES_WIDE, or the window opens in the
			 * band the sidebar degrades to initials in and the
			 * footer hint is clipped mid-word. A default, not a
			 * demand: the compositor's first configure wins on a
			 * screen too small for it.
			 */
			.cols = 104,
			.rows = 26,
		};
		if (kwl_init(&cfg) != 0) {
			fprintf(stderr, "kdos-res: no compositor — try --tty\n");
			return 1;
		}
	} else {
		ktui_backend_set(NULL);	/* NULL selects the built-in tty */
		if (ktui_term_init(1) != 0) {
			fprintf(stderr, "kdos-res: no terminal\n");
			return 1;
		}
	}
	ktui_draw_init();

	/*
	 * The default disposition for SIGHUP is death, so a program on
	 * reload_session()'s list that does not handle it is a program that
	 * gets KILLED by `kdos theme amber` and comes back looking retinted.
	 */
	signal(SIGHUP, on_hup);

	R.started_ms = kpr_mono_ms();
	unsigned long long next = 0;

	for (;;) {
		if (gui && kwl_should_close())
			break;

		if (g_reload) {
			g_reload = 0;
			res_conf_load();
			res_theme_from_cache();
			ktui_draw_invalidate();
		}

		unsigned long long now = kpr_mono_ms();
		if (now >= next) {
			res_sample();
			unsigned long long spent = kpr_mono_ms() - now;
			/*
			 * A tick that overran its own interval means this
			 * program is the load. It backs off and SAYS SO rather
			 * than quietly becoming the top consumer of the
			 * machine it is reporting on.
			 */
			R.overran = spent > (unsigned long long)RC.interval_ms;
			next = now + (unsigned long long)RC.interval_ms;
		}

		res_draw_frame();
		ktui_draw_flush();

		now = kpr_mono_ms();
		int wait = next > now ? (int)(next - now) : 0;

		KtuiEvent ev;
		if (!ktui_backend()->poll_event(&ev, wait)) {
			if (ktui_resized) {
				/*
				 * A resize is not applied until the consumer
				 * applies it: a loop that owns a surface and
				 * does not do this paints into a grid that no
				 * longer exists.
				 */
				ktui_resized = 0;
				ktui_draw_resize();
				ktui_draw_invalidate();
			}
			continue;
		}

		if (ev.type == KT_EVT_KEY) {
			if (res_frame_key(ev.key))
				break;
		} else if (ev.type == KT_EVT_MOUSE) {
			if (ev.press == KT_MP_DRAG)
				res_frame_motion(ev.mx, ev.my);
			else if (ev.press == KT_MP_PRESS &&
				 (ev.btn == KT_MB_WHEEL_UP ||
				  ev.btn == KT_MB_WHEEL_DOWN))
				res_frame_wheel(ev.btn == KT_MB_WHEEL_UP);
			else if (ev.press == KT_MP_PRESS)
				res_frame_click(ev.mx, ev.my, ev.btn);
			/* The one gesture on this surface that spans events:
			 * a scrollbar drag. Wayland reports plain motion and
			 * dragged motion identically, so the RELEASE is what
			 * ends it. */
			else if (ev.press == KT_MP_RELEASE)
				res_frame_release();
		}
	}

	if (gui)
		kwl_shutdown();
	else
		ktui_term_shutdown();
	return 0;
}
