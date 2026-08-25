/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   Boxes — the tenth page, and it needs no new subsystem
 * ---------------------------------
 *
 * libkproc already turns a pid into `firefox-esr (appbox kdos-apps)`, and
 * kdos-energyd, kdos-oomd and `kdos stutter` all identify boxes through the
 * same conmon walk. This page is a rollup over that walk keyed on the BOX
 * rather than on the application — which is why it is a page and not a
 * subsystem.
 *
 * WHAT IS ON IT AND WHAT IS NOT. Processes, CPU, memory and disk are measured
 * here. The ENERGY share is kdos-energyd's and is asked for rather than
 * recomputed — a second implementation of the RAPL attribution would
 * eventually disagree with the daemon's — and a `-` is what a machine with no
 * energy daemon gets, never a zero. A zero is how a monitor reports a sensor
 * that does not exist as a machine that is idle.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "res.h"

#define BOX_MAX 64

struct boxrow {
	char name[64];
	char base[48];
	char persist[12];
	int nproc;
	double cpu;
	unsigned long long rss;
	unsigned long long disk;	/* the writable upper                */
	unsigned long long uptime_s;	/* of the oldest member              */
	double energy;			/* percent, or -1 when unknown       */
	int running;
};

static struct boxrow g_box[BOX_MAX];
static int g_nbox;
static int g_sel;
static int g_top;
static int g_body = 1;
static int g_hover = -1;
static int g_dragbar;
static int g_follow = 1;
static int g_row0 = 1;
static int g_ox, g_oy, g_width;

enum { BS_NAME = 0, BS_CPU, BS_MEM, BS_DISK, BS_PROCS, BS_N };
static const char *const BS_NAMES[BS_N] = { "name", "cpu", "memory", "disk",
					    "procs" };
static const int BS_X[BS_N] = { 0, 20, 28, 38, 48 };
static int g_sort = BS_CPU;
static int g_rev;

static struct boxrow *find_box(const char *name)
{
	for (int i = 0; i < g_nbox; i++)
		if (!strcmp(g_box[i].name, name))
			return &g_box[i];
	if (g_nbox >= BOX_MAX)
		return NULL;
	memset(&g_box[g_nbox], 0, sizeof(g_box[0]));
	kb_strlcpy(g_box[g_nbox].name, name, sizeof(g_box[0].name));
	g_box[g_nbox].energy = -1.0;
	return &g_box[g_nbox++];
}

static int cmp_box(const void *A, const void *B)
{
	const struct boxrow *a = A, *b = B;
	int r = 0;

	switch (g_sort) {
	case BS_NAME:  r = strcmp(a->name, b->name); break;
	case BS_MEM:   r = a->rss < b->rss ? 1 : a->rss > b->rss ? -1 : 0; break;
	case BS_DISK:  r = a->disk < b->disk ? 1 : a->disk > b->disk ? -1 : 0;
		       break;
	case BS_PROCS: r = b->nproc - a->nproc; break;
	default:       r = a->cpu < b->cpu ? 1 : a->cpu > b->cpu ? -1 : 0; break;
	}
	return g_rev ? -r : r;
}

/*
 * What a box has WRITTEN — its overlay upper, walked. Not the packs beneath
 * it: those are shared between boxes and counting them per box would report
 * one 620 MB base six times. The walk is bounded because an upper is the
 * user's own changes; a box with a large one is a box that has been worked in.
 */
static unsigned long long dir_bytes(const char *path, int depth)
{
	char **names = kb_listdir(path, NULL);
	unsigned long long total = 0;

	if (depth > 12) {
		/* A symlink loop in somebody's upper is their business, not a
		 * reason for a monitor to stop responding. */
		kb_strv_free(names);
		return 0;
	}
	for (char **n = names; n && *n; n++) {
		char *q = kb_path_join(path, *n);
		struct stat st;

		if (lstat(q, &st) == 0) {
			if (S_ISDIR(st.st_mode))
				total += dir_bytes(q, depth + 1);
			else if (S_ISREG(st.st_mode))
				total += (unsigned long long)st.st_size;
		}
		free(q);
	}
	kb_strv_free(names);
	return total;
}

static unsigned long long upper_bytes(const char *box)
{
	char path[512];

	snprintf(path, sizeof(path), "%s/.local/share/kdos/boxes/%s",
		 kb_home_dir(), box);
	if (!kb_is_dir(path))
		return KPR_UNREADABLE;
	return dir_bytes(path, 0);
}

/*
 * kdos-energyd's own answer, asked for rather than recomputed: a second
 * implementation of the RAPL attribution would eventually disagree with the
 * daemon's, and the daemon is the one with the counter.
 */
static int energy_report(char *out, size_t n)
{
	const char *sp = getenv("KDOS_ENERGYD_SOCKET");
	struct sockaddr_un a;
	size_t got = 0;
	int fd;

	out[0] = 0;
	if (!sp || !*sp)
		sp = "/run/kdos-energyd.sock";
	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;
	memset(&a, 0, sizeof(a));
	a.sun_family = AF_UNIX;
	kb_strlcpy(a.sun_path, sp, sizeof(a.sun_path));
	if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
		close(fd);
		return -1;
	}
	if (write(fd, "report\n", 7) != 7) {
		close(fd);
		return -1;
	}
	for (;;) {
		ssize_t r = read(fd, out + got, n - got - 1);
		if (r <= 0)
			break;
		got += (size_t)r;
		if (got + 1 >= n)
			break;
	}
	out[got] = 0;
	close(fd);
	return got ? 0 : -1;
}

/*
 * The energy share, from the daemon that measures it. Read once per prepare
 * and cached for ten seconds: kdos-energyd samples on a fixed ten-second clock
 * of its own, so asking faster returns the same number and spends a socket
 * round trip on it.
 */
static void energy_fill(void)
{
	static double last_ms;
	static char cache[4096];
	char *line, *save;
	double now = (double)R.sample.wall_ms;

	if (!cache[0] || now - last_ms > 10000.0) {
		last_ms = now;
		if (energy_report(cache, sizeof(cache)) != 0)
			cache[0] = 0;
	}
	if (!cache[0])
		return;

	char buf[4096];
	kb_strlcpy(buf, cache, sizeof(buf));
	for (line = strtok_r(buf, "\n", &save); line;
	     line = strtok_r(NULL, "\n", &save)) {
		char *ap = strstr(line, "(appbox ");
		char name[64];
		char *close;
		double pct;

		if (!ap)
			continue;
		ap += 8;
		close = strchr(ap, ')');
		if (!close || (size_t)(close - ap) >= sizeof(name))
			continue;
		memcpy(name, ap, (size_t)(close - ap));
		name[close - ap] = 0;
		if (sscanf(close + 1, " %lf%%", &pct) != 1)
			continue;
		for (int i = 0; i < g_nbox; i++)
			if (!strcmp(g_box[i].name, name)) {
				if (g_box[i].energy < 0)
					g_box[i].energy = 0;
				g_box[i].energy += pct;
			}
	}
}

void res_box_prepare(void)
{
	static int applied;
	/*
	 * `starttime` is in clock ticks since boot and `wall_ms` is measured
	 * from the same moment, so the difference is the process's age without
	 * needing the boot wall time at all.
	 */
	long hz = kpr_hz();

	if (!applied) {
		applied = 1;
		for (int i = 0; i < BS_N; i++)
			if (!strcmp(RC.sort, BS_NAMES[i])) {
				g_sort = i;
				break;
			}
	}
	g_nbox = 0;

	for (int i = 0; i < R.sample.n; i++) {
		const KprProc *p = &R.sample.p[i];
		struct boxrow *b;
		const KprProc *prev;

		if (!p->box[0])
			continue;
		b = find_box(p->box);
		if (!b)
			continue;
		prev = R.have_prev ? kpr_find_pid(&R.prev, p->pid) : NULL;
		b->nproc++;
		b->running = 1;
		b->cpu += kpr_proc_cpu(prev, p, R.sample.wall_ms - R.prev.wall_ms);
		b->rss += p->rss;
		/*
		 * `starttime` is in clock ticks since BOOT, so a box's uptime
		 * is the boot time plus the earliest member's start. The
		 * oldest process in the box is the box's own age, because
		 * kdos-boxinit is pid 1 inside it and outlives everything.
		 */
		if (p->starttime && hz > 0) {
			unsigned long long age =
				R.sample.wall_ms / 1000 -
				p->starttime / (unsigned long long)hz;
			if (age > b->uptime_s)
				b->uptime_s = age;
		}
	}

	/*
	 * A BOX THAT IS DESCRIBED AND NOT RUNNING IS STILL A BOX. Leaving it
	 * out is how somebody creates the same one twice — the same reason
	 * `kdos-box list` reads the profiles as well as podman.
	 */
	{
		char dir[512];
		char **names;

		snprintf(dir, sizeof(dir), "%s/.config/kdos/boxes",
			 kb_home_dir());
		names = kb_listdir(dir, NULL);
		for (char **f = names; f && *f; f++) {
			char nm[64];
			size_t l = strlen(*f);

			if (l < 6 || strcmp(*f + l - 5, ".conf") ||
			    l - 5 >= sizeof(nm))
				continue;
			memcpy(nm, *f, l - 5);
			nm[l - 5] = 0;
			find_box(nm);
		}
		kb_strv_free(names);
	}

	for (int i = 0; i < g_nbox; i++) {
		char path[512], buf[2048];
		char *line, *save;

		g_box[i].disk = upper_bytes(g_box[i].name);
		kb_strlcpy(g_box[i].persist, "persistent",
			   sizeof(g_box[i].persist));
		snprintf(path, sizeof(path), "%s/.config/kdos/boxes/%s.conf",
			 kb_home_dir(), g_box[i].name);
		if (kb_read_file(path, buf, sizeof(buf)) < 0)
			continue;
		for (line = strtok_r(buf, "\n", &save); line;
		     line = strtok_r(NULL, "\n", &save)) {
			char *eq = strchr(line, '=');
			if (!eq)
				continue;
			*eq++ = 0;
			if (!strcmp(line, "base"))
				kb_strlcpy(g_box[i].base, eq,
					   sizeof(g_box[i].base));
			else if (!strcmp(line, "persistence"))
				kb_strlcpy(g_box[i].persist, eq,
					   sizeof(g_box[i].persist));
		}
	}
	energy_fill();

	qsort(g_box, (size_t)g_nbox, sizeof(g_box[0]), cmp_box);
	if (g_sel >= g_nbox)
		g_sel = g_nbox ? g_nbox - 1 : 0;
}

const char *res_box_headline(void)
{
	static char s[128];
	int running = 0;

	for (int i = 0; i < g_nbox; i++)
		running += g_box[i].running;
	snprintf(s, sizeof(s), "%d box%s, %d running", g_nbox,
		 g_nbox == 1 ? "" : "es", running);
	return s;
}

static void uptime_str(unsigned long long s, char *out, size_t n)
{
	if (!s) {
		kb_strlcpy(out, res_none(), n);
		return;
	}
	if (s < 3600)
		snprintf(out, n, "%llum", s / 60);
	else if (s < 86400)
		snprintf(out, n, "%lluh%02llum", s / 3600, (s % 3600) / 60);
	else
		snprintf(out, n, "%llud%02lluh", s / 86400, (s % 86400) / 3600);
}

void res_draw_boxes(int x, int y, int w, int h)
{
	const int bottom = y + h;
	int row = y;
	static const char *HDR[BS_N] = { "BOX", "CPU%", "MEMORY", "DISK",
					 "PROCS" };
	static const int HDR_W[BS_N] = { 19, 7, 9, 9, 6 };

	ktui_draw_fill(krect(x, row, w, 1), KT_SURFACE);
	for (int i = 0; i < BS_N; i++) {
		char lab[24];

		if ((i == BS_DISK && w < 47) || (i == BS_PROCS && w < 54))
			continue;
		snprintf(lab, sizeof(lab), "%s%s", HDR[i],
			 i == g_sort ? (g_rev ? ktui_glyph[KT_G_UP]
					      : ktui_glyph[KT_G_DOWN]) : "");
		ktui_draw_text(x + BS_X[i], row, HDR_W[i], lab,
			       i == g_sort ? KT_ACCENT : KT_MID, KT_SURFACE, 0);
	}
	/*
	 * The thresholds are the columns' own right edges, not a guess: PROCS
	 * ends at 54, ENERGY occupies 55..61 and UPTIME 63..70. Guessed ones
	 * (72 and 84) put both off the right edge of a 1280-pixel window,
	 * which is the shipped screen — found by LOOKING at it, which is what
	 * the golden harness is for and why a width test is arithmetic rather
	 * than a round number.
	 */
	if (w >= 62)
		ktui_draw_text(x + 55, row, 7, "ENERGY", KT_MID, KT_SURFACE, 0);
	if (w >= 71)
		ktui_draw_text(x + 63, row, 8, "UPTIME", KT_MID, KT_SURFACE, 0);
	row++;

	int body = bottom - row - 2;
	if (body < 1)
		return;
	g_body = body;
	kch_list_clamp(&g_top, g_sel, g_nbox, body, g_follow);

	for (int i = 0; i < body && g_top + i < g_nbox; i++) {
		const struct boxrow *b = &g_box[g_top + i];
		int is_sel = (g_top + i == g_sel);
		int is_hov = !is_sel && g_top + i == g_hover;
		int fg = is_sel ? KT_SURFACE : KT_TEXT;
		int bg = is_sel ? KT_ACCENT : is_hov ? KT_MID : KT_BG;
		int ry = row + i;
		char buf[64];

		if (is_sel || is_hov)
			ktui_draw_fill(krect(x, ry, w, 1), bg);

		/* A box that is not running says so where its state is read,
		 * not by being absent. */
		snprintf(buf, sizeof(buf), "%s%s", b->name,
			 b->running ? "" : " (stopped)");
		ktui_draw_text(x, ry, 20, buf, fg, bg, 0);

		if (b->running) {
			snprintf(buf, sizeof(buf), "%.1f", b->cpu);
			ktui_draw_text(x + BS_X[BS_CPU], ry, 7, buf, fg, bg, 0);
			ktui_draw_text(x + BS_X[BS_MEM], ry, 9, res_size(b->rss),
				       fg, bg, 0);
		} else {
			ktui_draw_text(x + BS_X[BS_CPU], ry, 7, res_none(), fg,
				       bg, 0);
			ktui_draw_text(x + BS_X[BS_MEM], ry, 9, res_none(), fg,
				       bg, 0);
		}
		if (w >= 47)
			ktui_draw_text(x + BS_X[BS_DISK], ry, 9,
				       b->disk == KPR_UNREADABLE ? res_none()
								 : res_size(b->disk),
				       fg, bg, 0);
		if (w >= 54) {
			snprintf(buf, sizeof(buf), "%d", b->nproc);
			ktui_draw_text(x + BS_X[BS_PROCS], ry, 6, buf, fg, bg, 0);
		}
		if (w >= 62) {
			/* `-`, never 0: no energy daemon is not no energy. */
			if (b->energy < 0)
				kb_strlcpy(buf, res_none(), sizeof(buf));
			else
				snprintf(buf, sizeof(buf), "%.1f%%", b->energy);
			ktui_draw_text(x + 55, ry, 7, buf, fg, bg, 0);
		}
		if (w >= 71) {
			uptime_str(b->uptime_s, buf, sizeof(buf));
			ktui_draw_text(x + 63, ry, 8, buf, fg, bg, 0);
		}
	}
	g_row0 = row - y;
	g_ox = x;
	g_oy = y;
	g_width = w;
	kch_scrollbar(0, x + w - 1, row, body, g_nbox, g_top, KT_BG);

	ktui_draw_text(x, bottom - 1, w,
		       "Enter for one box: its profile, its processes and the "
		       "verbs", KT_DIM, KT_BG, 0);
}

/* ── the pointer and the keys ──────────────────────────────────────────── */

int res_box_wheel(int up)
{
	if (!kch_list_wheel(up, &g_top, g_nbox, g_body)) {
		g_follow = 1;
		if (up && g_sel > 0)
			g_sel--;
		else if (!up && g_sel + 1 < g_nbox)
			g_sel++;
	} else {
		g_follow = 0;
	}
	return 1;
}

void res_box_motion(int mx, int my)
{
	int r = my - g_row0;

	if (g_dragbar) {
		/* A DRAG IS A PRESS THAT IS STILL DOWN. Wayland delivers plain
		 * motion and dragged motion identically — the event carries no
		 * button state at all — so the press is what has to be
		 * remembered. */
		int t = kch_scrollbar_drag(my + g_oy);

		if (t >= 0) {
			g_top = t;
			g_follow = 0;
		}
		return;
	}
	g_hover = -1;
	if (mx < 0 || mx >= g_width || r < 0 || r >= g_body)
		return;
	if (g_top + r < g_nbox)
		g_hover = g_top + r;
}

void res_box_release(void)
{
	g_dragbar = 0;
	kch_scrollbar_release();
}

static void open_detail(void)
{
	const struct boxrow *b;
	const char *lines[10];
	char l[10][96];
	int n = 0;

	if (g_sel < 0 || g_sel >= g_nbox)
		return;
	b = &g_box[g_sel];
	snprintf(l[n], sizeof(l[0]), "base         %s",
		 b->base[0] ? b->base : res_none());
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "persistence  %s", b->persist);
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "state        %s",
		 b->running ? "running" : "stopped");
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "processes    %d", b->nproc);
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "memory       %s",
		 b->running ? res_size(b->rss) : res_none());
	lines[n] = l[n]; n++;
	snprintf(l[n], sizeof(l[0]), "writable     %s",
		 b->disk == KPR_UNREADABLE ? res_none() : res_size(b->disk));
	lines[n] = l[n]; n++;
	if (b->energy >= 0) {
		snprintf(l[n], sizeof(l[0]), "energy       %.1f%% of "
			 "attributable", b->energy);
		lines[n] = l[n]; n++;
	}
	{
		char up[32];
		uptime_str(b->uptime_s, up, sizeof(up));
		snprintf(l[n], sizeof(l[0]), "uptime       %s", up);
		lines[n] = l[n]; n++;
	}
	res_detail_open_facts(b->name, "box", lines, n);
}

int res_box_click(int mx, int my, int btn)
{
	int r;

	if (btn != KT_MB_LEFT)
		return 0;

	/* The header is a row of sort controls, and a second press on the
	 * column already in force reverses it. */
	if (my == 0) {
		for (int i = BS_N - 1; i >= 0; i--)
			if (mx >= BS_X[i]) {
				if (g_sort == i)
					g_rev = !g_rev;
				else {
					g_sort = i;
					g_rev = 0;
				}
				return 1;
			}
		return 1;
	}
	{
		int t = kch_scrollbar_press(0, mx + g_ox, my + g_oy);

		if (t >= 0) {
			g_top = t;
			g_dragbar = 1;
			g_follow = 0;
			return 1;
		}
	}
	r = my - g_row0;
	if (r < 0 || r >= g_body || g_top + r >= g_nbox)
		return 0;
	/*
	 * A press on the row ALREADY selected opens it. A press rather than a
	 * double click because nothing in this toolkit measures one, and
	 * because opening a detail page is not destructive — the verbs are
	 * there, behind the confirm.
	 */
	if (g_top + r == g_sel) {
		open_detail();
	} else {
		g_sel = g_top + r;
		g_follow = 1;
	}
	return 1;
}

int res_box_key(int k)
{
	switch (k) {
	case KT_K_UP:
		if (g_sel > 0)
			g_sel--;
		g_follow = 1;
		return 1;
	case KT_K_DOWN:
		if (g_sel + 1 < g_nbox)
			g_sel++;
		g_follow = 1;
		return 1;
	case KT_K_ENTER:
		open_detail();
		return 1;
	case 's':
		g_sort = (g_sort + 1) % BS_N;
		return 1;
	case 'S':
		g_rev = !g_rev;
		return 1;
	default:
		return 0;
	}
}
