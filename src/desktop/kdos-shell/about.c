/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos-about — what this machine is
 *
 *   ╔═ About KDOS ═══════════════════════════════════════════════╗
 *   ║  ██╗  ██╗██████╗       KDOS 0.2                            ║
 *   ║  ██║ ██╔╝██╔══██╗      kernel    6.12.4                    ║
 *   ║  █████╔╝ ██║  ██║      libc      musl                      ║
 *   ║  ██╔═██╗ ██║  ██║      userland  toybox                    ║
 *   ║  ██║  ██╗██████╔╝      session   kdos-comp (phosphor)      ║
 *   ║  ╚═╝  ╚═╝╚═════╝       …                                   ║
 *   ╚════════════════════════════════════════════════════════════╝
 *
 * EVERY FACT IS READ, NOT FORKED. `uname`, `/proc/cpuinfo`, `/proc/meminfo`,
 * `/proc/uptime`, `/etc/os-release` and the package database are files this
 * process can open; spawning `fastfetch` to render them would put a second
 * program's layout, colours and ANSI on a surface that draws in slots, and
 * would make the About window the one surface that cannot be drawn offscreen
 * for a golden.
 * ---------------------------------
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/utsname.h>

#include "kbase.h"
#include "kwl.h"
#include "shell.h"

#define AB_ROWS_MAX 24
#define AB_VAL 96

struct fact {
	const char *key;
	char val[AB_VAL];
};

static struct fact facts[AB_ROWS_MAX];
static int nfacts;

/* No layers: Esc closes the card. */
static KtuiKeys keys;

static void fact(const char *key, const char *fmt, ...)
{
	if (nfacts >= AB_ROWS_MAX)
		return;

	va_list ap;

	facts[nfacts].key = key;
	va_start(ap, fmt);
	vsnprintf(facts[nfacts].val, sizeof(facts[nfacts].val), fmt, ap);
	va_end(ap);
	if (facts[nfacts].val[0])
		nfacts++;
}

/*
 * One `key="value"` out of a shell-style file, unquoted. os-release is the
 * only caller and its grammar is that small; a parser for the whole of it
 * would be a configuration library for one line.
 */
static int os_release(const char *key, char *out, size_t n)
{
	FILE *f = fopen("/etc/os-release", "r");
	char line[256];
	size_t klen = strlen(key);
	int got = 0;

	out[0] = '\0';
	if (!f)
		return -1;
	while (!got && fgets(line, sizeof(line), f)) {
		if (strncmp(line, key, klen) || line[klen] != '=')
			continue;

		char *v = line + klen + 1;

		v[strcspn(v, "\r\n")] = '\0';
		if (*v == '"') {
			v++;
			v[strcspn(v, "\"")] = '\0';
		}
		snprintf(out, n, "%s", v);
		got = 1;
	}
	fclose(f);
	return got ? 0 : -1;
}

/* The first `key<separator>value` line of a /proc file, value trimmed. */
static int proc_field(const char *path, const char *key, char *out, size_t n)
{
	FILE *f = fopen(path, "r");
	char line[512];
	size_t klen = strlen(key);
	int got = 0;

	out[0] = '\0';
	if (!f)
		return -1;
	while (!got && fgets(line, sizeof(line), f)) {
		if (strncmp(line, key, klen))
			continue;

		char *v = strchr(line, ':');

		if (!v)
			continue;
		v++;
		while (*v == ' ' || *v == '\t')
			v++;
		v[strcspn(v, "\r\n")] = '\0';
		snprintf(out, n, "%s", v);
		got = 1;
	}
	fclose(f);
	return got ? 0 : -1;
}

/* How many packages the database holds. One entry per directory. */
static int pkg_count(void)
{
	DIR *d = opendir("/var/lib/kpkg/db");
	struct dirent *e;
	int n = 0;

	if (!d)
		return -1;
	while ((e = readdir(d)))
		if (e->d_name[0] != '.')
			n++;
	closedir(d);
	return n;
}

static void gather(void)
{
	struct utsname u;
	char buf[AB_VAL];

	nfacts = 0;
	if (os_release("PRETTY_NAME", buf, sizeof(buf)) == 0)
		fact("", "%s", buf);
	if (os_release("VERSION", buf, sizeof(buf)) == 0)
		fact("version", "%s", buf);
	if (uname(&u) == 0)
		fact("kernel", "%s %s", u.release, u.machine);
	fact("libc", "musl");
	fact("userland", "toybox");

	fact("session", "kdos-comp (wayland)");
	fact("terminal", "%s", sh_term());

	/*
	 * NO GRID SIZE. A surface knows the cells it was given and not the
	 * ones the screen has, so reporting `ktui_w` here would print this
	 * window's own size under a name every reader takes for the desktop's.
	 * The display reports its grid where it knows it, and this window does
	 * not pretend to.
	 */

	if (proc_field("/proc/cpuinfo", "model name", buf, sizeof(buf)) == 0)
		fact("cpu", "%s", buf);
	if (proc_field("/proc/meminfo", "MemTotal", buf, sizeof(buf)) == 0) {
		long kb = atol(buf);

		if (kb > 0)
			fact("memory", "%ld MiB", kb / 1024);
	}

	FILE *f = fopen("/proc/uptime", "r");

	if (f) {
		double up = 0;

		if (fscanf(f, "%lf", &up) == 1 && up > 0) {
			long s = (long)up;

			if (s >= 3600)
				fact("uptime", "%ldh %ldm", s / 3600,
				     (s % 3600) / 60);
			else
				fact("uptime", "%ldm", s / 60);
		}
		fclose(f);
	}

	int pk = pkg_count();

	if (pk > 0)
		fact("packages", "%d", pk);
}

static char logo[SH_LOGO_LINES][SH_LOGO_BYTES];
static int logo_n, logo_w;

static void draw(void)
{
	int w = ktui_w, h = ktui_h;

	ktui_draw_fill(krect(0, 0, w, h), KT_SURFACE);
	ktui_draw_box(krect(0, 0, w, h), "About KDOS", KT_ACCENT, KT_SURFACE,
		      1);

	for (int i = 0; i < logo_n && 1 + i < h - 2; i++)
		ktui_draw_text(2, 1 + i, w - 4, logo[i], KT_ACCENT, KT_SURFACE,
			       KT_A_NONE);

	int kx = 2 + (logo_w ? logo_w + 3 : 0);

	for (int i = 0; i < nfacts && 1 + i < h - 2; i++) {
		int y = 1 + i;

		/* The name row carries no key and is the accent: it is the
		 * answer to "what is this", and the rest are its details. */
		if (!facts[i].key[0]) {
			ktui_draw_text(kx, y, w - kx - 2, facts[i].val,
				       KT_ACCENT, KT_SURFACE, KT_A_BOLD);
			continue;
		}
		ktui_draw_text(kx, y, 10, facts[i].key, KT_MID, KT_SURFACE,
			       KT_A_NONE);
		ktui_draw_text(kx + 10, y, w - kx - 12, facts[i].val, KT_TEXT,
			       KT_SURFACE, KT_A_NONE);
	}

	/*
	 * Only Esc is named. Enter and `q` stay bound and stay unsaid: they do
	 * the same one thing, and three keys for one verb is noise rather than
	 * a row that follows the focus.
	 */
	ktui_hint("Esc", ktui_esc_verb(&keys));
	ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);
}

static int on_event(KtuiEvent *ev)
{
	if (ev->type == KT_EVT_KEY && (ev->key == KT_K_ENTER || ev->key == 'q'))
		return SH_EV_CLOSE;
	if (ev->type == KT_EVT_MOUSE && ev->press == KT_MP_PRESS &&
	    ev->btn == KT_MB_RIGHT)
		return SH_EV_CLOSE;
	return SH_EV_PASS;
}

int about_main(int argc, char **argv)
{
	if (sh_logo_load("/usr/share/kdos/logo.txt", logo, SH_LOGO_LINES,
			 &logo_n, &logo_w) != 0)
		logo_n = logo_w = 0;	/* no artwork is not a failure */

	/*
	 * Sized from what there is to show rather than a constant: the logo is
	 * a file somebody can replace and the facts are however many of them
	 * this machine can answer. Two columns and a border, with the widest
	 * value clamped so a long CPU model does not size the window off the
	 * screen — the value is truncated there instead.
	 */
	gather();

	/* THREE, not two: a border top and bottom and the hint row above the
	 * lower one. The clamp below rises with it, or reserving the row would
	 * clip one more line of artwork instead of costing the window a row. */
	int rows = (logo_n > nfacts ? logo_n : nfacts) + 3;
	int cols = logo_w + 2 + 10 + 34 + 2;

	if (cols > 78)
		cols = 78;
	if (rows > 27)
		rows = 27;

	/* A dialog, not a dropdown: it is read rather than picked from, and
	 * clicking the window behind it to check something must not take it
	 * away. */
	const ShSurface s = {
		.cfg = {
			.role = KDISP_ROLE_OVERLAY,
			.cols = cols,
			.rows = rows,
			.app_id = "kdos-about",
			.keyboard = 1,
		},
		.keys = &keys,
		.popup = 1,
		.popup_bg = KT_SURFACE,
		.draw = draw,
		.event = on_event,
	};

	return sh_run(&s, argc, argv);
}
